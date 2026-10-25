"""Portable paths for virtual serial symlinks (no fixed /dev/ttyACM*).

Override with environment variables:
  VSA_STATE_DIR  — directory for bridge + terminal symlinks
  VSA_BRIDGE     — MQTT bridge side (mqtt_to_serial, virtual_serial TX)
  VSA_TERMINAL   — minicom / read_serial side
"""

import os


def state_dir():
    base = os.environ.get("VSA_STATE_DIR")
    if base:
        base = os.path.expanduser(base)
    else:
        runtime = os.environ.get("XDG_RUNTIME_DIR")
        if runtime:
            base = os.path.join(runtime, "virtual-serial-adaptor")
        else:
            base = os.path.join(
                os.path.expanduser("~"), ".local", "state", "virtual-serial-adaptor"
            )
    os.makedirs(base, mode=0o700, exist_ok=True)
    return base


def bridge_path():
    return os.environ.get("VSA_BRIDGE", os.path.join(state_dir(), "bridge"))


def terminal_path():
    return os.environ.get("VSA_TERMINAL", os.path.join(state_dir(), "terminal"))


def socat_pair_command():
    b, t = bridge_path(), terminal_path()
    return (
        f"socat pty,link={b},mode=600,raw,echo=0 "
        f"pty,link={t},mode=600,raw,echo=0"
    )
