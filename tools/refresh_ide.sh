#!/bin/bash
# Regenerate compile_commands.json for clangd after a build.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT/out/8915DM_cat1_open_release}"
python3 "$ROOT/tools/gen_compile_commands.py" "$OUT_DIR"
echo "Done. Restart clangd: Ctrl+Shift+P -> clangd: Restart language server"