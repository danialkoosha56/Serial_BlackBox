#!/usr/bin/env python3
"""Publish an incrementing counter via mosquitto_pub continuously."""

import argparse
import shutil
import subprocess
import sys
import time


def publish(host, port, topic, payload, username=None, password=None):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("topic", nargs="?", help="MQTT topic to publish to")
    parser.add_argument("--host", default="test.mosquitto.org", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="seconds between messages")
    parser.add_argument("--start", type=int, default=0, help="first counter value")
    parser.add_argument("--format", default="Hello {n}",
                        help="message template, {n} is the counter")
    parser.add_argument("--username", help="MQTT username")
    parser.add_argument("--password", help="MQTT password")
    args = parser.parse_args()

    if shutil.which("mosquitto_pub") is None:
        raise SystemExit("mosquitto_pub not found; install: sudo apt install mosquitto-clients")

    topic = args.topic or "my/test/uart/cmd"

    print(f"publishing to: {topic} ({args.host}:{args.port})", flush=True)

    counter = args.start
    try:
        while True:
            payload = args.format.format(n=counter)
            publish(args.host, args.port, topic, payload,
                    args.username, args.password)
            print(f"TX [{topic}] {payload}", flush=True)
            counter += 1
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nstopped", file=sys.stderr)
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f"mosquitto_pub failed with exit code {exc.returncode}")


if __name__ == "__main__":
    main()
