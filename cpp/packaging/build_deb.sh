#!/usr/bin/env bash
#
# Build a Debian binary package for Debug Overlay.
# Produces: cpp/build/packaging/debug-overlay_<version>_<arch>.deb
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BIN="$BUILD_DIR/debug_overlay_hud"
VERSION="${VERSION:-0.1.0}"
ARCH="$(dpkg --print-architecture)"
OUT="$BUILD_DIR/packaging"
STAGE="$OUT/debug-overlay_${VERSION}_${ARCH}"

if [ ! -x "$BIN" ]; then
  echo "Build the project first:" >&2
  echo "  cmake -S cpp -B cpp/build && cmake --build cpp/build -j" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
         "$STAGE/usr/bin" \
         "$STAGE/usr/lib/systemd/user" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/doc/debug-overlay"

install -m 0755 "$BIN" "$STAGE/usr/bin/debug-overlay"

sed 's|@BINARY@|/usr/bin/debug-overlay|g' \
  "$ROOT_DIR/packaging/debug-overlay.service.in" \
  > "$STAGE/usr/lib/systemd/user/debug-overlay.service"

cat > "$STAGE/usr/share/applications/debug-overlay.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Debug Overlay
Comment=Compact on-screen focused-process monitor
Exec=/usr/bin/debug-overlay
Terminal=false
Categories=Utility;System;Monitor;
EOF

install -m 0644 "$ROOT_DIR/packaging/README.md" \
  "$STAGE/usr/share/doc/debug-overlay/README.md"

cat > "$STAGE/DEBIAN/control" <<EOF
Package: debug-overlay
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libc6, libqt6widgets6, libqt6dbus6
Maintainer: Debug Overlay <noreply@example.com>
Description: Compact always-on-top HUD for focused-process and infra status
 Shows the focused application's PID, CPU, and RSS, plus optional Docker
 and Kubernetes health, in a small always-on-top overlay.
EOF

mkdir -p "$OUT"
dpkg-deb --build --root-owner-group "$STAGE" \
  "$OUT/debug-overlay_${VERSION}_${ARCH}.deb"

echo "Built: $OUT/debug-overlay_${VERSION}_${ARCH}.deb"
