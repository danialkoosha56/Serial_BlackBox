#!/usr/bin/env python3
"""Read lines from the virtual terminal side (see README.md)."""

import sys

import serial

from vsa_paths import terminal_path

port = sys.argv[1] if len(sys.argv) > 1 else terminal_path()
baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

with serial.Serial(port, baudrate, timeout=2) as ser:
    print(f"listening on {ser.name} @ {ser.baudrate}")
    while True:
        line = ser.readline()
        if line:
            print("RX:", line.decode(errors="replace").strip(), flush=True)
