# Simple virtual serial adaptor

PC-side tools for the **EC200 serial logger**: bridge MQTT to a pseudo-serial port (minicom/picocom), or generate fake UART traffic on the bench.

**Requires:** Linux, `mosquitto-clients` (`sudo apt install mosquitto-clients`).  
**Optional:** `pyserial` for `read_serial.py` (`pip install pyserial`).

Default MQTT broker/topics match **`serial_logger/mqtt_proc/mqtt_proc.c`**.

---

## Paths (portable, any PC)

Symlinks live under your home or runtime dir — **not** fixed names like `/dev/ttyACM100` (no root, no clashes with real USB adapters).

| Variable | Default if unset |
|----------|------------------|
| `VSA_STATE_DIR` | `$XDG_RUNTIME_DIR/virtual-serial-adaptor` or `~/.local/state/virtual-serial-adaptor` |
| `VSA_BRIDGE` | `$VSA_STATE_DIR/bridge` — attach **`mqtt_to_serial.py`** / **`virtual_serial.py`** here |
| `VSA_TERMINAL` | `$VSA_STATE_DIR/terminal` — open **minicom** / **`read_serial.py`** here |

Example on another machine:

```bash
export VSA_STATE_DIR="$HOME/tmp/my-vsa"
mkdir -p "$VSA_STATE_DIR"
# scripts pick up VSA_BRIDGE / VSA_TERMINAL under that dir automatically
```

---

## Usage

### 1. Remote serial (MQTT ↔ minicom)

**Terminal A — create PTY pair** (run once; leave open):

```bash
cd simple-virtual-serial-adaptor
python3 -c "from vsa_paths import socat_pair_command; print(socat_pair_command())" | sh
```

Or copy the printed command from step B if the bridge is not running yet.

Same thing explicitly:

```bash
socat pty,link="${VSA_BRIDGE:-$HOME/.local/state/virtual-serial-adaptor/bridge}",mode=600,raw,echo=0 \
      pty,link="${VSA_TERMINAL:-$HOME/.local/state/virtual-serial-adaptor/terminal}",mode=600,raw,echo=0
```

**Terminal B — MQTT bridge** (defaults: `test.mosquitto.org`, data/cmd topics):

```bash
python3 mqtt_to_serial.py
```

It prints the **bridge** path and a **minicom** line for the **terminal** path.

**Terminal C — serial monitor**

```bash
minicom -D "$(python3 -c 'from vsa_paths import terminal_path; print(terminal_path())')" -b 115200
```

- Module **log** MQTT → `data` topic → bytes into **bridge** → you see them on **terminal**.
- Each line you type in minicom → **cmd** topic → EC200 commands.

**Command replies** (`status`, etc.) are on a separate topic:

```bash
python3 mqtt_subscribe.py
# default topic: my/test/uart/answer
```

**Useful flags:** `--host`, `--port`, `--raw`, `--device`, positional `[subscribe_topic] [publish_topic]`.

---

### 2. Bench — fake device UART

Simulate the **target** printing on serial (into EC200 UART1 via USB-serial wired to **bridge** side):

```bash
# Terminal A: socat pair (as above)
python3 virtual_serial.py --device "$(python3 -c 'from vsa_paths import bridge_path; print(bridge_path())')"
# or, after socat, write into bridge path shown by mqtt_to_serial / vsa_paths

python3 read_serial.py    # listens on terminal side
```

---

### 3. Quick MQTT tests (no serial)

```bash
python3 mqtt_publish.py              # publishes "Hello n" to cmd topic
python3 mqtt_subscribe.py my/test/uart/data
```

---

## MQTT topics (EC200 firmware)

| Topic | Direction (module) | Tool |
|-------|-------------------|------|
| `my/test/uart/data` | module → MQTT | Bridge **subscribes** → serial |
| `my/test/uart/cmd` | MQTT → module | Bridge **publishes** ← serial |
| `my/test/uart/answer` | module → MQTT | `mqtt_subscribe.py` |

---

## Files

| File | Role |
|------|------|
| **`vsa_paths.py`** | Shared paths + `socat_pair_command()` |
| **`mqtt_to_serial.py`** | Main MQTT ↔ bridge serial |
| **`mqtt_subscribe.py`** | Print one topic |
| **`mqtt_publish.py`** | Test publisher |
| **`virtual_serial.py`** | Fake counter on PTY / `--device` |
| **`read_serial.py`** | Read lines from terminal path |
