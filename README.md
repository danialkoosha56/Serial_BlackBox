# EC200 Serial Data Logger (OpenCPU overlay)

Firmware and PC tools for the **Quectel EC200U** serial logger (`serial_logger/` → `ql_app_serial_logger` → app image **`V01_APP`**).

This repository does **not** include the Quectel OpenCPU SDK. You must obtain the SDK from Quectel and apply this overlay on top.

## Tested SDK baseline

| Item | Value |
|------|--------|
| Module / project | **EC200UEU_AA** |
| SDK reference | **EC200UEUAAR03A14M08** (8915DM Cat-1 OpenCPU) |
| Build label | **V01** (third argument to `build_all.sh`) |

Use the **same SDK generation** if possible. Newer Quectel releases may need small merge fixes in the overlay files under `sdk-overlay/components/`.

## Quick start (developer with SDK)

1. Unpack the official Quectel SDK somewhere, e.g. `~/quectel/EC200U-R03A14M08/`.
2. Clone or copy **this repo** (only this folder contents).
3. Run:

```bash
./apply-to-sdk.sh ~/quectel/EC200U-R03A14M08/
```

4. Build from the **SDK root**:

```bash
cd ~/quectel/EC200U-R03A14M08/
./build_all.sh new EC200UEU_AA V01
```

5. Outputs appear under `target/EC200UEU_AA_V01/` (merged `.pac`, `V01_APP.pac`, etc.).

See also `dani-addition.txt` for Docker / QDloader flash commands.

## What is in this repo

| Path | Purpose |
|------|---------|
| **`sdk-overlay/serial_logger/`** | Application source (`ql_app_serial_logger`) |
| **`sdk-overlay/components/...`** | Minimal Quectel tree edits (init link, SMS log hook, demos, CMake) |
| **`patches/root-CMakeLists-add-serial_logger.patch`** | Adds `add_subdirectory_if_exist(serial_logger)` at SDK root |
| **`MANIFEST-sdk-overlay.txt`** | List of every file copied by `apply-to-sdk.sh` |
| **`simple-virtual-serial-adaptor/`** | PC-side MQTT ↔ virtual serial |
| **`tools/build_and_program.sh`** | Optional: build + flash wrapper (run from SDK root after overlay) |

Full product description: [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md).

## GitHub layout

You can publish **this directory alone** as the GitHub repository root (no `components/`, no `prebuilts/`). Alternatively ship the release archive:

```bash
tar -czf ec200-serial-logger.tar.gz -C .. ec200-serial-logger
```

## Privacy note

`sdk-overlay/components/ql-application/sms/sms_demo.c` contains a hard-coded test phone number from development. Change or remove it before sharing publicly if needed.

## License

Application code in `sdk-overlay/serial_logger/` is yours to license. Quectel SDK files remain subject to Quectel’s terms when you apply the overlay.
