#!/bin/sh
# Syspeek installer.
#
# One-line install (Linux, glibc >= 2.34, x86-64):
#   curl -fsSL https://raw.githubusercontent.com/codeafridi/Syspeek/main/install.sh | sh
#
# Downloads a self-contained build (Qt 6 bundled — no Qt or compiler needed),
# installs it to ~/.local/opt/syspeek, adds a `syspeek` command, starts it at
# login, and installs the desktop focus bridge (GNOME extension / KWin script)
# when one is needed.
#
# Options:
#   --no-autostart   install but do not enable start-at-login
#   --no-bridge      skip installing the desktop focus bridge
#   --uninstall      remove the app, command, service, autostart and bridge
#   -h, --help       show this help
#
# Environment overrides:
#   SYSPEEK_DIR         install directory (default ~/.local/opt/syspeek)
#   SYSPEEK_BUNDLE_URL  override the download URL
#
set -eu

REPO="codeafridi/Syspeek"
RAW="https://raw.githubusercontent.com/${REPO}/main"
BUNDLE_URL="${SYSPEEK_BUNDLE_URL:-https://github.com/${REPO}/releases/latest/download/syspeek-portable.tar.gz}"

HOME_DIR="${HOME:-/root}"
APP_DIR="${SYSPEEK_DIR:-$HOME_DIR/.local/opt/syspeek}"
BIN_DIR="${XDG_BINDIR:-$HOME_DIR/.local/bin}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME_DIR/.config}"
UNIT_DIR="$CONFIG_HOME/systemd/user"
UNIT_FILE="$UNIT_DIR/syspeek.service"
AUTOSTART_DIR="$CONFIG_HOME/autostart"
AUTOSTART_FILE="$AUTOSTART_DIR/syspeek.desktop"
WRAPPER="$BIN_DIR/syspeek"

GNOME_UUID="syspeek-focus@codeafridi"
KDE_ID="syspeek-focus"

autostart=1
bridge=1
mode=install

for arg in "$@"; do
  case "$arg" in
    --uninstall)    mode=uninstall ;;
    --no-autostart) autostart=0 ;;
    --no-bridge)    bridge=0 ;;
    -h|--help)      sed -n '2,22p' "$0" 2>/dev/null || true; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

log() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

fetch() { # <url> <dest>
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$1" -o "$2"
  elif command -v wget >/dev/null 2>&1; then
    wget -qO "$2" "$1"
  else
    die "need curl or wget to download the bundle"
  fi
}

have_systemd() {
  command -v systemctl >/dev/null 2>&1 && systemctl --user show-environment >/dev/null 2>&1
}

desktop_id() {
  printf '%s' "${XDG_CURRENT_DESKTOP:-}${XDG_SESSION_DESKTOP:-}" | tr '[:upper:]' '[:lower:]'
}

write_wrapper() {
  mkdir -p "$BIN_DIR"
  cat > "$WRAPPER" <<EOF
#!/bin/sh
set -eu
APP="\${SYSPEEK_DIR:-\$HOME/.local/opt/syspeek}"
UNIT="syspeek.service"
cmd="\${1:-run}"
[ "\$#" -gt 0 ] && shift
case "\$cmd" in
  run|gui)   exec "\$APP/run.sh" "\$@" ;;
  start)     systemctl --user start "\$UNIT" ;;
  stop)      systemctl --user stop "\$UNIT" 2>/dev/null || pkill -f "\$APP/syspeek" 2>/dev/null || true ;;
  restart)   systemctl --user restart "\$UNIT" ;;
  status)    systemctl --user status "\$UNIT" --no-pager 2>/dev/null || pgrep -af "\$APP/syspeek" || echo "not running" ;;
  logs)      journalctl --user -u "\$UNIT" -f ;;
  enable)    systemctl --user enable --now "\$UNIT" ;;
  disable)   systemctl --user disable --now "\$UNIT" ;;
  update)    curl -fsSL ${RAW}/install.sh | sh ;;
  uninstall) curl -fsSL ${RAW}/install.sh | sh -s -- --uninstall ;;
  path)      echo "\$APP" ;;
  *) echo "usage: syspeek {run|start|stop|restart|status|logs|enable|disable|update|uninstall|path}" >&2; exit 2 ;;
esac
EOF
  chmod 0755 "$WRAPPER"
}

install_autostart() {
  if have_systemd; then
    mkdir -p "$UNIT_DIR"
    cat > "$UNIT_FILE" <<EOF
[Unit]
Description=Syspeek HUD
[Service]
ExecStart=$APP_DIR/run.sh
Restart=on-failure
[Install]
WantedBy=default.target
EOF
    systemctl --user daemon-reload
    systemctl --user import-environment DISPLAY WAYLAND_DISPLAY \
      XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_RUNTIME_DIR 2>/dev/null || true
    if [ "$autostart" -eq 1 ]; then
      systemctl --user enable --now syspeek.service
    else
      systemctl --user start syspeek.service 2>/dev/null || true
    fi
    log "Installed systemd user service: $UNIT_FILE"
  else
    mkdir -p "$AUTOSTART_DIR"
    cat > "$AUTOSTART_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=Syspeek
Exec=$APP_DIR/run.sh
Terminal=false
X-GNOME-Autostart-enabled=true
EOF
    log "Installed XDG autostart entry: $AUTOSTART_FILE"
    if [ "$autostart" -eq 1 ]; then
      ( setsid "$APP_DIR/run.sh" >/dev/null 2>&1 & ) || true
    fi
    log "note: no systemd user session; stop with: pkill -f syspeek"
  fi
}

enable_gnome_extension() { # <uuid>
  command -v gsettings >/dev/null 2>&1 || return 0
  cur="$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || echo '[]')"
  case "$cur" in
    *"'$1'"*) return 0 ;;
  esac
  case "$cur" in
    ''|'[]'|'@as []') nv="['$1']" ;;
    *) nv="${cur%]}, '$1']" ;;
  esac
  gsettings set org.gnome.shell enabled-extensions "$nv" 2>/dev/null || true
}

disable_gnome_extension() { # <uuid>
  command -v gsettings >/dev/null 2>&1 || return 0
  cur="$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || echo '[]')"
  nv="$(printf '%s' "$cur" | sed "s/, *'$1'//; s/'$1', *//; s/'$1'//")"
  case "$nv" in
    *'['*']'*) gsettings set org.gnome.shell enabled-extensions "$nv" 2>/dev/null || true ;;
  esac
}

bridge_gnome() {
  src="$APP_DIR/bridge/gnome/$GNOME_UUID"
  dst="$HOME_DIR/.local/share/gnome-shell/extensions/$GNOME_UUID"
  if [ ! -f "$src/metadata.json" ]; then
    log "note: GNOME focus bridge not bundled; focus will show '--' until installed."
    return 0
  fi
  mkdir -p "$dst"
  cp -f "$src/metadata.json" "$src/extension.js" "$dst/" 2>/dev/null || true
  enable_gnome_extension "$GNOME_UUID"
  log "Installed GNOME focus bridge -> $dst"
  log "  GNOME loads new extensions only at login: log out and back in to activate it."
}

bridge_kde() {
  src="$APP_DIR/bridge/kwin/$KDE_ID"
  dst="$HOME_DIR/.local/share/kwin/scripts/$KDE_ID"
  if [ ! -f "$src/metadata.json" ]; then
    log "note: KWin focus bridge not bundled."
    return 0
  fi
  rm -rf "$dst"
  mkdir -p "$dst"
  cp -r "$src/." "$dst/" 2>/dev/null || true
  if command -v kpackagetool6 >/dev/null 2>&1; then
    kpackagetool6 --type=KWin/Script -i "$dst" >/dev/null 2>&1 || true
  elif command -v kpackagetool5 >/dev/null 2>&1; then
    kpackagetool5 --type=KWin/Script -i "$dst" >/dev/null 2>&1 || true
  fi
  if command -v kwriteconfig6 >/dev/null 2>&1; then
    kwriteconfig6 --file kwinrc --group Plugins --key "${KDE_ID}Enabled" true
  elif command -v kwriteconfig5 >/dev/null 2>&1; then
    kwriteconfig5 --file kwinrc --group Plugins --key "${KDE_ID}Enabled" true
  fi
  if command -v qdbus6 >/dev/null 2>&1; then
    qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
  elif command -v qdbus >/dev/null 2>&1; then
    qdbus org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
  fi
  log "Installed KDE focus bridge."
}

install_focus_bridge() {
  [ "$bridge" -eq 1 ] || return 0
  case "$(desktop_id)" in
    *gnome*) bridge_gnome ;;
    *kde*|*plasma*) bridge_kde ;;
    *sway*)   log "note: focus detection needs 'swaymsg'; install it if you don't have it." ;;
    *niri*)   log "note: focus detection needs 'niri msg'; install it if you don't have it." ;;
    *hyprland*) log "note: focus detection needs 'hyprctl'; install it if you don't have it." ;;
    *xfce*|*x11*|*mate*|*cinnamon*) log "note: on X11, focus detection needs 'xdotool'." ;;
    *) log "note: focus detection on X11 needs 'xdotool'; GNOME/KDE need the desktop bridge." ;;
  esac
}

remove_focus_bridge() {
  if [ -d "$HOME_DIR/.local/share/gnome-shell/extensions/$GNOME_UUID" ]; then
    disable_gnome_extension "$GNOME_UUID"
    rm -rf "$HOME_DIR/.local/share/gnome-shell/extensions/$GNOME_UUID"
  fi
  rm -rf "$HOME_DIR/.local/share/kwin/scripts/$KDE_ID"
}

do_uninstall() {
  if command -v systemctl >/dev/null 2>&1; then
    systemctl --user disable --now syspeek.service 2>/dev/null || true
  fi
  remove_focus_bridge
  rm -f "$UNIT_FILE" "$AUTOSTART_FILE" "$WRAPPER"
  [ -n "${APP_DIR:-}" ] && [ -d "$APP_DIR" ] && rm -rf "$APP_DIR"
  if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload 2>/dev/null || true
  fi
  log "Syspeek uninstalled."
}

if [ "$mode" = "uninstall" ]; then
  do_uninstall
  exit 0
fi

command -v tar >/dev/null 2>&1 || die "tar is required"

arch="$(uname -m 2>/dev/null || echo unknown)"
[ "$arch" = "x86_64" ] || log "warning: this bundle is built for x86_64; you are on $arch"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT INT TERM

log "Downloading Syspeek..."
fetch "$BUNDLE_URL" "$tmp/bundle.tar.gz" \
  || die "download failed ($BUNDLE_URL). If there is no release yet, build from source (see README)."

rm -rf "$APP_DIR"
mkdir -p "$APP_DIR"
tar xzf "$tmp/bundle.tar.gz" -C "$APP_DIR"
chmod +x "$APP_DIR/run.sh" "$APP_DIR/syspeek" 2>/dev/null || true

write_wrapper
install_autostart
install_focus_bridge

log ""
log "Installed to $APP_DIR"
cat <<'EOF' >&2
Control it:
  syspeek run         run in the foreground
  syspeek stop        stop it now
  syspeek disable     stop it and don't start at login
  syspeek enable      start it and start at login
  syspeek status      is it running?
  syspeek update      upgrade to the latest version
  syspeek uninstall   remove everything
EOF

case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *) log ""; log "note: add $BIN_DIR to your PATH to use the 'syspeek' command." ;;
esac
