#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"

if [[ -x "$ROOT_DIR/venv/bin/python" ]]; then
  PYTHON_BIN="$ROOT_DIR/venv/bin/python"
fi

# xdotool enables automatic focus tracking on X11. The overlay can still run
# without it by selecting a process with its TARGET button.
if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  if ! command -v xdotool >/dev/null 2>&1; then
    echo "xdotool is unavailable: use the TARGET button to choose a PID, or install xdotool for automatic focus tracking." >&2
  fi
fi

exec "$PYTHON_BIN" "$ROOT_DIR/overlay_design.py"
