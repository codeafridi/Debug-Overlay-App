# Syspeek — build, install, and desktop integration

This directory contains the C++20 / Qt 6 implementation of Syspeek and
its packaging.

## Build prerequisites

- A C++20 compiler (GCC 11+ or Clang 14+)
- CMake 3.16+
- Qt 6 Widgets and Qt 6 DBus development packages
- Linux `/proc` (standard)

Package names (typical):

| Distribution | Packages |
| --- | --- |
| Debian / Ubuntu / Mint | `build-essential cmake qt6-base-dev libqt6dbus6` |
| Fedora | `gcc-c++ cmake qt6-qtbase-devel qt6-qtbase` (DBus via `qt6-qtbase`) |
| Arch / CachyOS | `base-devel cmake qt6-base` |

X11 focus detection additionally needs `xdotool` (X11/XWayland sessions only).
Wayland sessions use compositor integrations (below) instead.

## Build

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
ctest --test-dir cpp/build --output-on-failure
```

## Install / uninstall

```bash
bash cpp/install.sh            # systemd user service if available
bash cpp/install.sh --no-systemd   # XDG autostart only
bash cpp/install.sh --uninstall
```

- Installs the binary to `~/.local/bin/syspeek` (`XDG_BINDIR` overrides).
- With systemd: writes `~/.config/systemd/user/syspeek.service` and enables it.
- Without systemd: writes `~/.config/autostart/syspeek.desktop`.

## Focus detection per desktop

Syspeek is PID-centric; each desktop supplies the focused PID through its
own channel:

| Session | Mechanism |
| --- | --- |
| GNOME (Wayland/X11) | GNOME Shell extension (`gnome_extension/syspeek-focus@codeafridi`) |
| KDE Plasma (Wayland) | KWin script + D-Bus (`cpp/kwin_script`, `cpp/install_kde_focus.sh`) |
| Hyprland | `hyprctl` |
| Sway | `swaymsg` |
| X11 / XWayland (Cinnamon, XFCE, MATE, KDE-X11) | `xdotool` |
| Other Wayland compositors | not supported (shows `no-focus`) |

There is no generic Wayland way to obtain the focused PID (the foreign-toplevel
protocols expose only `app_id`/`title`), so compositor-specific adapters are
required.

## Configuration

Optional INI file at `~/.config/syspeek/config.ini`
(`SYSPEEK_CONFIG` overrides the path). See the project README / feature
report for the full key list. Missing file or invalid values fall back to
defaults.

## Distribution packages

Source templates are provided; only the Debian package is buildable in this
environment.

| Format | Artifact | Build command |
| --- | --- | --- |
| Debian/Ubuntu (`.deb`) | `cpp/packaging/build_deb.sh` | `bash cpp/packaging/build_deb.sh` |
| Fedora/RHEL (`.rpm`) | `cpp/packaging/rpm/syspeek.spec` | `rpmbuild -ba cpp/packaging/rpm/syspeek.spec` |
| Arch (PKGBUILD) | `cpp/packaging/arch/PKGBUILD` | `makepkg -si` |

The `.deb` installs the binary to `/usr/bin/syspeek`, a system-wide user
unit to `/usr/lib/systemd/user/syspeek.service`, a launcher `.desktop`, and
docs. Per-user autostart is then a normal user action:

```bash
systemctl --user enable --now syspeek.service   # systemd
# or add ~/.config/autostart/syspeek.desktop     (non-systemd / desktop)
```

Other distributions are **not yet packaged** (Arch packages must be built on
Arch; RPM on an RPM-based distro). The source build + `install.sh` works
everywhere Qt 6 is available.

