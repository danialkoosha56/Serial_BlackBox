#!/usr/bin/env bash
# Copy this project's overlay onto an official Quectel EC200U OpenCPU SDK tree.
#
# Usage:
#   ./apply-to-sdk.sh /path/to/quectel-sdk
#
# Then build from the SDK root (example):
#   ./build_all.sh new EC200UEU_AA V01

set -euo pipefail

SDK_ROOT="${1:-}"
if [[ -z "$SDK_ROOT" || ! -d "$SDK_ROOT" ]]; then
    echo "Usage: $0 /path/to/quectel-sdk" >&2
    exit 1
fi

SDK_ROOT="$(cd "$SDK_ROOT" && pwd)"
PKG_DIR="$(cd "$(dirname "$0")" && pwd)"
OVERLAY="$PKG_DIR/sdk-overlay"

for marker in build_all.sh components/ql-config CMakeLists.txt; do
    if [[ ! -e "$SDK_ROOT/$marker" ]]; then
        echo "Error: $SDK_ROOT does not look like a Quectel OpenCPU SDK (missing $marker)." >&2
        exit 1
    fi
done

echo "SDK root: $SDK_ROOT"
echo "Overlay:  $OVERLAY"

# Application + modified SDK files (same paths as inside the SDK)
while IFS= read -r relpath; do
    [[ -z "$relpath" ]] && continue
    src="$OVERLAY/$relpath"
    dest="$SDK_ROOT/$relpath"
    mkdir -p "$(dirname "$dest")"
    cp -a "$src" "$dest"
done < "$PKG_DIR/MANIFEST-sdk-overlay.txt"

# Root CMakeLists.txt: register top-level serial_logger/
if grep -q 'add_subdirectory_if_exist(serial_logger)' "$SDK_ROOT/CMakeLists.txt"; then
    echo "CMakeLists.txt already lists serial_logger/ — skipping patch."
else
    patch -d "$SDK_ROOT" -p1 --forward < "$PKG_DIR/patches/root-CMakeLists-add-serial_logger.patch"
fi

echo ""
echo "Done. Next (from SDK root):"
echo "  ./build_all.sh new EC200UEU_AA V01"
echo "  ./build_all.sh r EC200UEU_AA V01    # incremental rebuild"
echo ""
echo "Flash (host tool in this repo): see README.md"
