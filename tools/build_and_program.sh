#!/bin/bash
# Build firmware, refresh IDE index, then flash to the module.
#
# Usage:
#   ./tools/build_and_program.sh new EC200UEU_AA V01 [/dev/ttyUSB0]
#
# Same args as build_all.sh, plus optional serial port as last argument.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PORT=""
args=()
for arg in "$@"; do
    case "$arg" in
        /dev/tty*)
            PORT="$arg"
            ;;
        *)
            args+=("$arg")
            ;;
    esac
done

if [ ${#args[@]} -lt 3 ]; then
    echo "Usage: $0 new EC200UEU_AA V01 [/dev/ttyUSB0]" >&2
    exit 1
fi

buildver="${args[2]}"
buildver="${buildver^^}"
project="${args[1]}"

./build_all.sh "${args[@]}"

QLOADER_PATH="${QLOADER_PATH:-$ROOT/QDloader_EC200/out}"
FIRMWARE_PATH="${FIRMWARE_PATH:-$ROOT/target/${project}_${buildver}}"
PORT="${PORT:-${QDLOADER_PORT:-/dev/ttyUSB0}}"
PAC="${FIRMWARE_PATH}/8915DM_cat1_open_${buildver}_merge.pac"

if [ ! -x "${QLOADER_PATH}/QDloader" ]; then
    echo "QDloader not found: ${QLOADER_PATH}/QDloader" >&2
    exit 1
fi

if [ ! -f "$PAC" ]; then
    echo "Firmware not found: $PAC" >&2
    exit 1
fi

echo "Programming $PAC on $PORT ..."
exec "${QLOADER_PATH}/QDloader" -s "$PORT" -f "$PAC" -g EC200U
