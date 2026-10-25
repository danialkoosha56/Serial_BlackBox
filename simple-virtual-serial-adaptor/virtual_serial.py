#!/usr/bin/env python3
"""Create a virtual serial port and stream an incrementing counter into it.

Writes to a PTY or to an existing bridge path (see vsa_paths / README.md).
"""

import argparse
import errno
import os
import pty
import signal
import sys
import termios
import time

from vsa_paths import bridge_path, terminal_path


def configure_raw(fd, baudrate):
    """Put the pty line discipline in 8N1 raw mode with echo disabled."""
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


def create_link(link_path, target):
    if os.path.islink(link_path):
        os.unlink(link_path)
    elif os.path.exists(link_path):
        raise SystemExit(f"{link_path} already exists and is not a symlink")

    try:
        os.symlink(target, link_path)
    except PermissionError:
        raise SystemExit(f"cannot create {link_path}; re-run with sudo")


def stream_counter(fd, args, label):
    counter = args.start
    while True:
        payload = (args.format.format(n=counter) + args.eol).encode()
        try:
            os.write(fd, payload)
            if not args.quiet:
                print("TX:", payload.decode().strip(), flush=True)
        except BlockingIOError:
            termios.tcflush(fd, termios.TCOFLUSH)
        except OSError as exc:
            if exc.errno != errno.EIO:
                raise
        counter += 1
        time.sleep(args.interval)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device",
                        help="write to an existing port (use with socat pairs)")
    parser.add_argument("--link", default=None,
                        help="symlink for PTY (default: $VSA_BRIDGE path)")
    parser.add_argument("--baudrate", type=int, default=115200,
                        help="reported line speed (default: %(default)s)")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="seconds between messages (default: %(default)s)")
    parser.add_argument("--start", type=int, default=0,
                        help="first counter value (default: %(default)s)")
    parser.add_argument("--format", default="Hello {n}",
                        help="message template, {n} is the counter")
    parser.add_argument("--eol", default="\r\n",
                        choices=["\r\n", "\n", "\r"],
                        help="line terminator (default: CRLF)")
    parser.add_argument("--quiet", action="store_true",
                        help="do not echo transmitted lines to stdout")
    args = parser.parse_args()
    if args.link is None:
        args.link = bridge_path()

    if args.device:
        fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY)
        configure_raw(fd, args.baudrate)
        print(f"sending counter to existing port: {args.device}")
        print(f"read the other end with minicom or read_serial.py")
        try:
            stream_counter(fd, args, args.device)
        finally:
            os.close(fd)
        return

    master_fd, slave_fd = pty.openpty()
    pts_name = os.ttyname(slave_fd)

    configure_raw(slave_fd, args.baudrate)
    os.chmod(pts_name, 0o666)
    os.set_blocking(master_fd, False)

    create_link(args.link, pts_name)

    def cleanup(*_):
        if os.path.islink(args.link) and os.readlink(args.link) == pts_name:
            os.unlink(args.link)
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    print(f"virtual serial port ready: {args.link} -> {pts_name}")
    print(f"read other end of socat pair: python3 read_serial.py")
    print(f"          or: minicom -D {terminal_path()} -b {args.baudrate}")

    try:
        stream_counter(master_fd, args, args.link)
    finally:
        cleanup()


if __name__ == "__main__":
    main()
