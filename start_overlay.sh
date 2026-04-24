#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"

if [[ -x "$ROOT_DIR/venv/bin/python" ]]; then
  PYTHON_BIN="$ROOT_DIR/venv/bin/python"
fi

# Only require xdotool on X11 (not on Wayland)
if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  if ! command -v xdotool >/dev/null 2>&1; then
    echo "xdotool is required for X11. Install it first and then run this script again." >&2
    exit 1
  fi
fi

exec "$PYTHON_BIN" "$ROOT_DIR/overlay_design.py"
