#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="$ROOT_DIR/systemd/debug-overlay.service.template"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_FILE="$CONFIG_DIR/debug-overlay.service"

if ! command -v systemctl >/dev/null 2>&1; then
  echo "systemctl is required to install automatic startup." >&2
  exit 1
fi

mkdir -p "$CONFIG_DIR"
sed "s|@PROJECT_DIR@|$ROOT_DIR|g" "$TEMPLATE" > "$UNIT_FILE"

# Make the current graphical-session variables available to the user service.
dbus-update-activation-environment --systemd \
  DISPLAY WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE || true

systemctl --user daemon-reload
systemctl --user enable --now debug-overlay.service

echo "Debug Overlay now starts at login and restarts if it exits."
echo "Status: systemctl --user status debug-overlay.service"
echo "Stop:   systemctl --user disable --now debug-overlay.service"
