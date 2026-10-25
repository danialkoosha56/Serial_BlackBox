#!/usr/bin/env python3
"""Bidirectional bridge between MQTT and a serial port.

- subscribe topic -> serial port (write)
- serial port (read)  -> publish topic
"""

import argparse
import os
import select
import shutil
import signal
import subprocess
import sys
import termios
import threading

from vsa_paths import bridge_path, socat_pair_command, terminal_path

# Defaults aligned with serial_logger/mqtt_proc/mqtt_proc.c
DEFAULT_HOST = "test.mosquitto.org"
DEFAULT_PORT = 1883
DEFAULT_SUBSCRIBE_TOPIC = "my/test/uart/data"
DEFAULT_PUBLISH_TOPIC = "my/test/uart/cmd"


def configure_raw(fd, baudrate):
    speed = getattr(termios, f"B{baudrate}", None)
    if speed is None:
        raise SystemExit(f"unsupported baudrate: {baudrate}")

    iflag, oflag, cflag, lflag, _, _, cc = termios.tcgetattr(fd)

    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP
               | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHOE | termios.ECHONL | termios.ICANON
               | termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL

    termios.tcsetattr(fd, termios.TCSANOW,
                      [iflag, oflag, cflag, lflag, speed, speed, cc])


def open_serial(device, baudrate):
    if not os.path.exists(device):
        raise SystemExit(
            f"{device} not found.\n"
            "Create a virtual port pair first (no root if using default paths):\n"
            f"  {socat_pair_command()}\n"
            "See README.md — paths: VSA_STATE_DIR, VSA_BRIDGE, VSA_TERMINAL."
        )
    fd = os.open(device, os.O_RDWR | os.O_NOCTTY)
    configure_raw(fd, baudrate)
    return fd


def mqtt_sub_cmd(host, port, topic, username, password):
    cmd = [
        "mosquitto_sub",
        "-h", host,
        "-p", str(port),
        "-t", topic,
        "-v",
    ]
    if username:
        cmd.extend(["-u", username])
    if password:
        cmd.extend(["-P", password])
    return cmd


def mqtt_publish(host, port, topic, payload, username, password):
    cmd = [
        "mosquitto_pub",
        "-h", host,
        "-p", str(port),
        "-t", topic,
        "-m", payload,
    ]
    if username:
        cmd.extend(["-u", username])
    if password:
        cmd.extend(["-P", password])
    subprocess.run(cmd, check=True)


def parse_mqtt_line(line, subscribe_topic):
    line = line.rstrip("\n")
    if not line:
        return None
    if line.startswith(subscribe_topic + " "):
        return line[len(subscribe_topic) + 1:]
    if " " in line:
        return line.split(" ", 1)[1]
    return line


def extract_serial_messages(buffer):
    messages = []
    while buffer:
        idx = -1
        skip = 0
        for sep in (b"\r\n", b"\n", b"\r"):
            pos = buffer.find(sep)
            if pos != -1 and (idx == -1 or pos < idx):
                idx = pos
                skip = len(sep)
        if idx == -1:
            break
        messages.append(buffer[:idx])
        buffer = buffer[idx + skip:]
    return messages, buffer


def mqtt_to_serial_loop(proc, serial_fd, args, stop_event):
    try:
        for line in proc.stdout:
            if stop_event.is_set():
                break

            payload = parse_mqtt_line(line, args.subscribe_topic)
            if payload is None:
                continue

            data = payload.encode() if args.raw else (payload + args.eol).encode()
            os.write(serial_fd, data)
            print(f"MQTT -> Serial: {payload}", flush=True)
    finally:
        stop_event.set()


def serial_to_mqtt_loop(serial_fd, args, stop_event):
    buffer = b""
    try:
        while not stop_event.is_set():
            ready, _, _ = select.select([serial_fd], [], [], 0.2)
            if serial_fd not in ready:
                continue

            chunk = os.read(serial_fd, 4096)
            if not chunk:
                continue

            if args.raw:
                payload = chunk.decode(errors="replace")
                mqtt_publish(args.host, args.port, args.publish_topic, payload,
                             args.username, args.password)
                print(f"Serial -> MQTT: {payload}", flush=True)
                continue

            buffer += chunk
            messages, buffer = extract_serial_messages(buffer)
            for message in messages:
                payload = message.decode(errors="replace")
                mqtt_publish(args.host, args.port, args.publish_topic, payload,
                             args.username, args.password)
                print(f"Serial -> MQTT: {payload}", flush=True)
    except OSError:
        pass
    finally:
        stop_event.set()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("subscribe_topic", nargs="?",
                        help="MQTT topic to subscribe (MQTT -> serial)")
    parser.add_argument("publish_topic", nargs="?",
                        help="MQTT topic to publish (serial -> MQTT)")
    parser.add_argument("--device", default=None,
                        help="bridge-side path (default: $VSA_BRIDGE or under ~/.local/state/...)")
    parser.add_argument("--host", default=DEFAULT_HOST, help="MQTT broker host")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="MQTT broker port")
    parser.add_argument("--baudrate", type=int, default=115200, help="serial baud rate")
    parser.add_argument("--eol", default="\r\n", choices=["\r\n", "\n", "\r"],
                        help="append to MQTT->serial messages (default: CRLF)")
    parser.add_argument("--raw", action="store_true",
                        help="no line endings; forward bytes as-is both ways")
    parser.add_argument("--username", help="MQTT username")
    parser.add_argument("--password", help="MQTT password")
    args = parser.parse_args()

    if shutil.which("mosquitto_sub") is None or shutil.which("mosquitto_pub") is None:
        raise SystemExit("install mosquitto clients: sudo apt install mosquitto-clients")

    args.subscribe_topic = args.subscribe_topic or DEFAULT_SUBSCRIBE_TOPIC
    args.publish_topic = args.publish_topic or DEFAULT_PUBLISH_TOPIC

    device = args.device or bridge_path()
    serial_fd = open_serial(device, args.baudrate)
    stop_event = threading.Event()

    proc = subprocess.Popen(
        mqtt_sub_cmd(args.host, args.port, args.subscribe_topic,
                     args.username, args.password),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    def stop(*_):
        stop_event.set()
        proc.terminate()
        os.close(serial_fd)
        sys.exit(0)

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    print(f"MQTT subscribe : {args.subscribe_topic}", flush=True)
    print(f"MQTT publish   : {args.publish_topic}", flush=True)
    print(f"Serial         : {device} @ {args.baudrate}", flush=True)
    print(f"Open terminal  : minicom -D {terminal_path()} -b {args.baudrate}", flush=True)
    print("bridge running... (Ctrl+C to stop)", flush=True)

    serial_thread = threading.Thread(
        target=serial_to_mqtt_loop,
        args=(serial_fd, args, stop_event),
        daemon=True,
    )
    serial_thread.start()

    try:
        mqtt_to_serial_loop(proc, serial_fd, args, stop_event)
        err = proc.stderr.read()
        if err:
            print(err, file=sys.stderr, end="")
        proc.wait()
    finally:
        stop_event.set()
        proc.terminate()
        serial_thread.join(timeout=1)
        os.close(serial_fd)


if __name__ == "__main__":
    main()
