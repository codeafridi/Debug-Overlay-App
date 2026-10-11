#!/usr/bin/env bash
#
# build_appimage.sh - build a single-file AppImage from the portable bundle.
#
# Reuses the self-contained, bundled-Qt output of build_portable.sh, so the
# resulting AppImage runs on any glibc >= 2.34 x86-64 distro with no Qt or
# compiler installed. appimagetool is downloaded on first use; Docker is only
# needed if the portable bundle has not been built yet.
#
# Usage:
#   bash cpp/packaging/build_appimage.sh [output_dir]
#
# Environment:
#   APPIMAGETOOL_URL  override the appimagetool download URL
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
out_dir="${1:-$repo_root/dist}"
mkdir -p "$out_dir"

version="$(git -C "$repo_root" describe --tags --abbrev=0 2>/dev/null || echo 0.0.0)"
version="${version#v}"
arch="$(uname -m)"
[ "$arch" = "x86_64" ] || { echo "error: AppImage target is x86_64, you are on $arch" >&2; exit 1; }

bundle="$out_dir/syspeek-portable.tar.gz"
appdir="$out_dir/Syspeek.AppDir"
appimage="$out_dir/Syspeek-${version}-${arch}.AppImage"
tool_dir="$out_dir/.tools"
appimagetool="$tool_dir/appimagetool-x86_64.AppImage"
appimagetool_url="${APPIMAGETOOL_URL:-https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage}"

log() { printf '>> %s\n' "$*" >&2; }

# Force extraction-based execution of appimagetool so this works without FUSE
# (containers, CI, WSL are all common cases).
export APPIMAGE_EXTRACT_AND_RUN=1

# 1. Portable bundle (bundled Qt). Build it if missing.
if [ ! -f "$bundle" ]; then
  log "portable bundle not found; building it (requires Docker)"
  bash "$here/build_portable.sh" "$out_dir"
fi

# 2. Assemble the AppDir.
log "assembling AppDir"
rm -rf "$appdir"
mkdir -p "$appdir/usr" "$appdir/usr/share/applications" \
         "$appdir/usr/share/icons/hicolor/scalable/apps"

tar xzf "$bundle" -C "$appdir/usr"

# Bundle layout is flat (syspeek, lib/, plugins/, run.sh, bridge/, README.md).
# Move the binary under usr/bin; lib/ and plugins/ already sit at usr/.
mkdir -p "$appdir/usr/bin"
mv "$appdir/usr/syspeek" "$appdir/usr/bin/syspeek"
chmod +x "$appdir/usr/bin/syspeek"
rm -f "$appdir/usr/run.sh"

cp "$here/icons/syspeek.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/syspeek.svg"
cp "$here/icons/syspeek.svg" "$appdir/syspeek.svg"

cat > "$appdir/syspeek.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Syspeek
Comment=Compact on-screen focused-process monitor
Exec=syspeek
Icon=syspeek
Terminal=false
Categories=Utility;System;Monitor;
X-AppImage-Version=${version}
EOF
cp "$appdir/syspeek.desktop" "$appdir/usr/share/applications/syspeek.desktop"

cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
exec "$HERE/usr/bin/syspeek" "$@"
EOF
chmod +x "$appdir/AppRun"

# 3. appimagetool.
if [ ! -x "$appimagetool" ]; then
  mkdir -p "$tool_dir"
  log "downloading appimagetool"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$appimagetool_url" -o "$appimagetool"
  elif command -v wget >/dev/null 2>&1; then
    wget -qO "$appimagetool" "$appimagetool_url"
  else
    echo "error: need curl or wget to download appimagetool" >&2
    exit 1
  fi
  chmod +x "$appimagetool"
fi

# 4. Emit the AppImage.
log "building AppImage"
if ! ARCH="$arch" "$appimagetool" --no-appstream "$appdir" "$appimage" >/dev/null 2>&1; then
  ARCH="$arch" "$appimagetool" "$appdir" "$appimage"
fi
chmod +x "$appimage" 2>/dev/null || true

log "done: $appimage"
log "run:  $appimage"
