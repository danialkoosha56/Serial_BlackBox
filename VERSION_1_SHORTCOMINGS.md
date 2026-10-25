# EC200 Serial Logger — Version 1 Shortcomings

Review scope: `serial_logger/` application layer (Quectel EC200UEU firmware).  
Purpose: serial data capture on **UART1**, optional MQTT publish and SD storage, remote config via **UART2** (debug), MQTT, and SMS.

---

## Architecture (summary)

| Path | Role |
|------|------|
| UART1 | Serial **data** input → ring buffer → MQTT / SD |
| UART2 | Debug log + **commands** (`dani_log`) |
| MQTT | Publish log data; subscribe cmd topic; publish answers |
| SD | Append logs under `SD:/SERIAL_LOG/` (when enabled at boot) |
| SMS | Whitelisted sender only; commands + text replies |
| Settings | `UFS:/setting.txt` (struct + CRC32) |

Tasks: `sys_proc`, `IN_LOG_PROC`, `dani_mqtt`, `sd_proc`, `sms_proc`, `io_proc`.

---

## Critical / release-risk issues

### 1. Settings mostly not connected to runtime

Many commands persist fields that runtime code does not read:

| Setting | Used at runtime? |
|---------|------------------|
| `uartbaud` | **No** — UART1 and UART2 stay **115200** hardcoded |
| `capturesize` | **No** — flush uses fixed 2048-byte chunks |
| `capturetime` | **Partly** — logger timer hardcoded **10 s**; SD file rotation uses setting |
| `mqtt`, broker URL/port/user/pass | **No** — hardcoded Mosquitto URL and empty MQTT credentials |
| `brokertopic` | **No** — publish/subscribe topics fixed in `mqtt_proc.c` |
| `mqtt enable/disable` | **No** — MQTT task always runs |
| `sd_card enable` | **Boot only** — toggling later does not restart SD task |
| `cmd2device` | **No** — remote commands not gated |
| `output1` (stored state) | **No** — only `setpin` drives GPIO |
| `phone_number` | SMS whitelist — default **empty** → **all SMS ignored** |

**Impact:** CLI looks complete; device behavior is largely compile-time constants. Highest gap for a “configurable serial logger” v1.

### 2. Binary serial data treated as C strings

`input_log_proc` logs capture buffers with `%s` without guaranteeing a trailing `NUL`. Binary streams can over-read, corrupt logs, or fault.

MQTT publish passes explicit `len` (good), but logging and string-oriented tooling remain unsafe for binary payloads.

### 3. Silent data loss on the logger path

- Ring buffer: full buffer → `ring_buffer_write` returns 0; **no error log**.
- MQTT/SD queues: full → drop; callers **ignore** post return values.

Under high serial rate or slow network/SD, **data can vanish** with little visibility.

### 4. Command parser fragility

- Commands matched with `strstr()` (substring false positives possible).
- `cmd_args()` assumes the command name is at the **start** of the line.
- Single static line buffer in `cmd_procc` — concurrent MQTT/SMS/UART commands could clash (no lock).

### 5. UART command replies not sent

`cmd_ans()` for `CMD_SRC_UART` logs only; does **not** write responses to UART2. MQTT and SMS get replies.

### 6. SMS provisioning

Empty `phone_number` rejects all SMS until `phonenumber` is set via UART/MQTT. **`phonenumber` is missing from `help` text.**

### 7. `output1` vs help text

Help: `output1 high/low`. Handler: **`enable` / `disable` only**.

---

## Important incomplete or misleading features

- **`brokertopic`** — one stored field; firmware uses three hardcoded topics (`data`, `cmd`, `answer`).
- **Broker auth** — user/password saved but not passed to MQTT connect.
- **`log_pkt_t` metadata** — timestamps / flags largely unused on wire or SD.
- **SD** — default off; 15 s boot wait; mount failure **ends task** until reboot.
- **SMS boot** — deletes **all** SIM messages on startup.
- **`io_proc`** — fixed GPIO10/11, ADC0; no pinmux setup; not linked to `output1` setting.
- **FOTA** — not implemented in `serial_logger/`.
- **Security (v1)** — public broker, no TLS, open cmd topic naming; knowledge of topics ≈ remote control.

---

## Code quality notes

- All commands live in `command.c` (split TODO still commented).
- Naming typos: `cmd_parsser`, `cmd_procc`, `permision`, inconsistent `output1` macro casing.
- Command callback return values inconsistent and unused.
- Duplicated magic numbers vs settings defaults.
- `setting_print(set)` uses unbounded `strcpy` into caller buffer.
- UART2 RX: single read per notify (may need drain loop under load).
- No automated tests under `serial_logger/`.

---

## What works well for v1

- Clean separation: data UART vs command/debug UART.
- Ring buffer + timer + 75% high-water wake is a sound logging pattern (once settings and binary handling align).
- Settings file + CRC persistence on UFS.
- SMS whitelist, reply routing, and long-SMS chunking (after number is configured).
- SD log files named/rotated by RTC.

---

## Suggested v1.0 checklist (before calling it “release”)

1. Wire **baud**, **capture size/time**, and **MQTT** settings — or remove/document non-functional commands.
2. Make logging and paths **binary-safe** (no `%s` on raw buffers).
3. Log or surface **overflow** (ring buffer, MQTT/SD queues).
4. Fix command parsing (line-start token match); document reply channels.
5. Align **help** with behavior (`output1`, `phonenumber`, MQTT topics vs `brokertopic`).
6. Product decision: SD default, mount retry, SMS wipe-on-boot, SMS/MQTT security model.

---

## Known limitations (document for users)

- Configure SMS with `phonenumber +98…` over UART or MQTT before SMS commands work.
- MQTT broker and topics in firmware may not match saved `broker*` settings until wired in code.
- Logger UART baud is 115200 until `uartbaud` is implemented in `input_log_proc`.
- SD logging requires `sdcard enable` **before reboot** if disabled at first boot.

---

*Generated from codebase review — EC200-newV / `serial_logger/`.*
