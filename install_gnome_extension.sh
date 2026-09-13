#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UUID="debug-overlay-focus@codeafridi"
SOURCE_DIR="$ROOT_DIR/gnome_extension/$UUID"

if ! command -v gnome-extensions >/dev/null 2>&1; then
  echo "gnome-extensions was not found. Run this from a GNOME desktop session." >&2
  exit 1
fi
if ! command -v zip >/dev/null 2>&1; then
  echo "zip was not found. Install zip, then run this installer again." >&2
  exit 1
fi

bundle_dir="$(mktemp -d)"
trap 'rm -rf "$bundle_dir"' EXIT

bundle_path="$bundle_dir/$UUID.shell-extension.zip"
(
  cd "$SOURCE_DIR"
  zip -q -r "$bundle_path" metadata.json extension.js
)

gnome-extensions install --force "$bundle_path"
gnome-extensions enable "$UUID"

echo "Installed and enabled $UUID in this GNOME session. Run ./start_overlay.sh."
