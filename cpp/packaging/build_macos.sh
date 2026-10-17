#!/bin/sh
# Build the Syspeek .app bundle and a drag-install DMG on macOS.
#
# Usage: SYSPEEK_BUILD_DIR=cpp/build bash cpp/packaging/build_macos.sh
#   SYSPEEK_BUILD_DIR  (default cpp/build)  cmake build dir to (re)configure
#   SYSPEEK_ARCHS      (default "arm64;x86_64") CMAKE_OSX_ARCHITECTURES
#   SYSPEEK_DMG_DIR    (default $SYSPEEK_BUILD_DIR/packaging)
#   SYSPEEK_SKIP_DEPLOY=1 skips macdeployqt (portable dev-tree run only)
#
# Bundle contract (cpp/packaging/Info.plist.in): APPL, dev.syspeek.hud,
# LSUIElement=1, LSMinimumSystemVersion 12.0, executable name "syspeek".

set -eu
cd "$(dirname "$0")/../.."

BUILD_DIR="${SYSPEEK_BUILD_DIR:-cpp/build}"
ARCHS="${SYSPEEK_ARCHS:-arm64;x86_64}"
OUT_DIR="${SYSPEEK_DMG_DIR:-${BUILD_DIR}/packaging}"

VERSION="$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\)[[:space:]]*$/\1/p' cpp/CMakeLists.txt | head -1)"
[ -n "$VERSION" ] || { echo "cannot parse project version from cpp/CMakeLists.txt" >&2; exit 1; }

APP="${BUILD_DIR}/syspeek.app"

cmake -S cpp -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

[ -d "$APP" ] || { echo "missing bundle: $APP" >&2; exit 1; }
plutil -lint "${APP}/Contents/Info.plist"

if [ "${SYSPEEK_SKIP_DEPLOY:-0}" != "1" ]; then
    MDQT="$(command -v macdeployqt || true)"
    if [ -z "$MDQT" ] && [ -n "${QT_ROOT_DIR:-}" ]; then
        MDQT="$(find "$QT_ROOT_DIR" -name macdeployqt -type f 2>/dev/null | head -1 || true)"
    fi
    [ -n "$MDQT" ] || { echo "macdeployqt not found (PATH/QT_ROOT_DIR)" >&2; exit 1; }
    "$MDQT" "$APP" -always-overwrite
fi

STAGE="$(mktemp -d "${TMPDIR:-/tmp}/syspeek-dmg.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT
cp -R "$APP" "$STAGE/Syspeek.app"
ln -s /Applications "$STAGE/Applications"

OUT_DIR="${OUT_DIR%/}"
mkdir -p "$OUT_DIR"
DMG="$OUT_DIR/Syspeek-${VERSION}-macos.dmg"
rm -f "$DMG" "${DMG}.sha256"
hdiutil create -volname Syspeek -srcfolder "$STAGE" -ov -format UDZO "$DMG"
shasum -a 256 "$DMG" > "${DMG}.sha256"
echo "built: $DMG"
