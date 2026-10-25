#!/usr/bin/env python3
"""Subscribe to an MQTT topic via mosquitto_sub and print every message."""

import argparse
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("topic", nargs="?", help="MQTT topic to subscribe to")
    parser.add_argument("--host", default="test.mosquitto.org", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--username", help="MQTT username")
    parser.add_argument("--password", help="MQTT password")
    args = parser.parse_args()

    if shutil.which("mosquitto_sub") is None:
        raise SystemExit("mosquitto_sub not found; install: sudo apt install mosquitto-clients")

    topic = args.topic or "my/test/uart/answer"

    cmd = [
        "mosquitto_sub",
        "-h", args.host,
        "-p", str(args.port),
        "-t", topic,
        "-v",
    ]
    if args.username:
        cmd.extend(["-u", args.username])
    if args.password:
        cmd.extend(["-P", args.password])

    print(f"listening on: {topic} ({args.host}:{args.port})", flush=True)
    try:
        subprocess.run(cmd, check=False)
    except KeyboardInterrupt:
        print("\nstopped", file=sys.stderr)


if __name__ == "__main__":
    main()
