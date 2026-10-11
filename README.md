<div align="center">

# Debug Overlay

### A tiny, always-on-top HUD for the app you're focused on — CPU, RAM, and Docker/Kubernetes status at a glance.

[![Platform: Linux](https://img.shields.io/badge/platform-Linux-informational?style=flat-square&logo=linux&logoColor=white)](#install)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)](#install)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt&logoColor=white)](#install)
[![License: MIT](https://img.shields.io/badge/license-MIT-f4c430?style=flat-square)](LICENSE)

[Install](#install) · [Use](#use) · [GNOME / KDE setup](#gnome--kde-setup) · [Docker & Kubernetes](#docker--kubernetes) · [Troubleshooting](#troubleshooting)

<sub>Repository: <strong>Syspeek</strong></sub>

<br />

<img src="docs/hud.png" alt="Debug Overlay HUD showing the focused app's name, PID, CPU and RSS plus Docker and Kubernetes badges" width="430" />

</div>

---

No more digging through `top`. Debug Overlay follows the window you're using and
shows that app's CPU and RAM, plus optional Docker/Kubernetes health, in a small
panel that stays on top and out of your way.

## Install

**1. Get the build tools** (Qt 6 Widgets + DBus dev, CMake, a C++20 compiler):

| Distribution | Command |
| --- | --- |
| Ubuntu / Debian / Mint | `sudo apt install build-essential cmake qt6-base-dev` |
| Fedora / RHEL | `sudo dnf install gcc-c++ cmake qt6-qtbase-devel` |
| Arch / CachyOS | `sudo pacman -S --needed base-devel cmake qt6-base` |
| openSUSE | `sudo zypper install gcc-c++ cmake qt6-base-devel` |

**2. Build it:**

```bash
git clone https://github.com/codeafridi/Syspeek.git
cd Syspeek
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
```

**3. Install it so it starts at login:**

```bash
bash cpp/install.sh
```

That's it. The overlay is running now and will start automatically whenever you
log in.

- Prefer no systemd service? `bash cpp/install.sh --no-systemd` (uses XDG autostart).
- To remove it: `bash cpp/install.sh --uninstall`.
- Just try it without installing: `./cpp/build/debug_overlay_hud`.

> On **GNOME** or **KDE** you also need a one-time focus bridge — see
> [GNOME / KDE setup](#gnome--kde-setup). Everything else works out of the box.

## Use

- **Drag** the panel anywhere (grab it, including the buttons).
- **`MORE` / `LESS`** — open/close the diagnosis panel.
- **`HIDE` / `SHOW`** — collapse to a single line or expand.
- **Click `docker:` or `k8s:`** — expand that badge's details.
- **`Ctrl+C`** in the terminal — quit.

## GNOME / KDE setup

Focus detection works on GNOME, KDE, Sway, niri, and X11 out of the box. On
**GNOME** and **KDE** the focused app is shared through a small bridge you install
once:

**GNOME** (then log out and back in):

```bash
bash install_gnome_extension.sh
```

**KDE Plasma (Wayland):**

```bash
bash cpp/install_kde_focus.sh
```

On **Sway / niri / Hyprland / X11**, install the matching tool (`swaymsg` /
`niri` / `hyprctl` / `xdotool`) and you're done. Other Wayland compositors run
fine but show `no-focus`.

## Docker & Kubernetes

Both are optional. If the tool isn't installed or reachable, the overlay shows a
status instead of failing.

- **Docker** — shows running count and flags unhealthy, restarting, and recently
  exited containers.
- **Kubernetes** — flags problem pods (`CrashLoopBackOff`, image-pull failures,
  `OOMKilled`) and deployments with too few ready replicas, using your current
  `kubectl` context.

## Advanced (optional)

Thresholds and polling intervals are configurable in
`~/.config/debug-overlay/config.ini`; sensible defaults are used if it's absent.

Handy environment variables:

- `DEBUG_OVERLAY_PROBE=1 ./debug-overlay` — print the detected focus provider and
  focused PID, then exit.
- `DEBUG_OVERLAY_LOG=/tmp/overlay.log` — log status to a file.

## Troubleshooting

**The app column shows `--` / `no-focus`.** Focus detection isn't available yet.

1. Run the probe to see which provider was chosen:

   ```bash
   DEBUG_OVERLAY_PROBE=1 ./cpp/build/debug_overlay_hud
   ```

2. Set up the bridge for your desktop (see
   [GNOME / KDE setup](#gnome--kde-setup)) and, on GNOME, **log out and back in**.

**It builds but won't start.** Check the platform plugin:

```bash
QT_DEBUG_PLUGINS=1 ./cpp/build/debug_overlay_hud 2>&1 | tail
```

## License

[MIT](LICENSE).
