<div align="center">

# Debug Overlay

### A lightweight Qt 6 HUD for the focused Linux application's CPU, RSS, and optional container health.

[![Platform: Linux](https://img.shields.io/badge/platform-Linux-informational?logo=linux&logoColor=white)](#requirements)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](#build-from-source)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](#requirements)
[![Build system: CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)](#build-from-source)

[Features](#features) · [Install](#install) · [Usage](#usage) · [Architecture](#architecture) · [Testing](#testing)

</div>

```text
┌ DEBUG HUD ─────────────────────────────────────────────── OK ┐
│ App            PID          CPU          MEM (RSS)           │
│ cursor         2092707      12%          212 MB              │
│ docker: 1 running  
  kubernetes :reachable                                   HIDE │
└──────────────────────────────────────────────────────────────┘
```

Debug Overlay is a small, always-on-top Linux desktop monitor. Rather than
making you search through every process in `top`, it follows the window you are
currently using and shows a compact view of that application's CPU and resident
memory use. It can also show optional Docker and Kubernetes status without
putting those slower integrations on the UI thread.

> The original Python/Tkinter implementation remains in this repository. The
> C++20 / Qt 6 rewrite lives in [`cpp/`](cpp/).

## Features

- Focused-app monitoring: application name, PID, CPU, and RSS.
- Process-group aggregation for helper processes, with PID start time used to
  avoid confusing a reused PID with an earlier process.
- Compact `OK`, `WARN`, and `HIGH CPU` health state based on sustained samples.
- Frameless, always-on-top Qt Widgets HUD that does not intentionally take
  keyboard focus.
- On-demand details: `MORE` opens the diagnosis panel; `HIDE` switches to one
  compact line.
- Optional Docker status, including running count and unhealthy, restarting,
  and recently exited containers.
- Optional Kubernetes status, including selected pod failure reasons and
  unavailable deployment replicas.
- Per-source worker threads, bounded command output, deadlines, retry backoff,
  and stale-result handling for Docker and Kubernetes.
- Central, validated INI configuration for health thresholds and polling.
- User-level installation with a systemd service where available, with XDG
  autostart fallback.

## What it is not

This is deliberately not a replacement for a full dashboard, `htop`, Docker
Desktop, or Kubernetes observability stack. Its normal view is intended to stay
small and glanceable. Docker and Kubernetes details open only when requested.

CPU/RSS are focused-application measurements. Docker and Kubernetes values are
separate infrastructure signals; they are not attributed to the focused app.
RSS is summed across the selected process group, so shared memory can be counted
more than once.

## Requirements

- Linux with `/proc`
- CMake 3.16 or newer
- A C++20 compiler
- Qt 6 Widgets and Qt 6 DBus development packages

Typical package names:

| Distribution | Packages |
| --- | --- |
| Debian / Ubuntu / Mint | `build-essential cmake qt6-base-dev` |
| Fedora | `gcc-c++ cmake qt6-qtbase-devel` |
| Arch | `base-devel cmake qt6-base` |
| openSUSE | `gcc-c++ cmake qt6-base-devel` |

Optional integrations:

- `xdotool` for X11/XWayland focus detection.
- `docker` for Docker status.
- `kubectl` with an accessible current context for Kubernetes status.

## Build from source

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
ctest --test-dir cpp/build --output-on-failure
```

Run the development build:

```bash
./cpp/build/debug_overlay_hud
```

## Install

After building:

```bash
# Install to ~/.local/bin and enable the user service when available.
bash cpp/install.sh

# Use XDG desktop autostart instead of a systemd user service.
bash cpp/install.sh --no-systemd

# Remove the installed binary and startup entry.
bash cpp/install.sh --uninstall
```

The project also contains a Debian package builder plus RPM and Arch packaging
templates. See [`cpp/packaging/README.md`](cpp/packaging/README.md).

## Usage

- Drag the HUD to reposition it.
- Select **MORE** / **LESS** to show or hide the diagnosis panel.
- Select **HIDE** / **SHOW** to switch between regular and compact display.
- Select a visible `docker:` or `k8s:` line to expand its details.
- Press `Ctrl+C` when running from a terminal to quit.

### Check focus detection

This probe prints the selected provider and the currently focused PID without
starting the normal HUD:

```bash
DEBUG_OVERLAY_PROBE=1 ./cpp/build/debug_overlay_hud
```

Example:

```text
provider=gnome-pidfile
focused_pid=1313
focused_comm=code
```

## Focus detection

Wayland does not offer a single standard API that exposes the focused window's
PID. Debug Overlay chooses an adapter from the current session environment.

| Desktop/session | Adapter | Extra setup |
| --- | --- | --- |
| GNOME | GNOME Shell PID-file bridge | Install the included GNOME extension, then log out/in |
| KDE Plasma Wayland | KWin script + D-Bus bridge | Run `cpp/install_kde_focus.sh` |
| Hyprland | `hyprctl` | `hyprctl` must be available |
| Sway | `swaymsg` | `swaymsg` must be available |
| niri | `niri msg` | `niri` must be available |
| X11 / XWayland | `xdotool` | Install `xdotool` |
| Other Wayland compositors | No adapter | HUD runs with `no-focus` |

For GNOME setup, use the repository's existing bridge installer:

```bash
bash install_gnome_extension.sh
```

## Docker and Kubernetes

Both sources are optional. Missing tools are represented as unavailable/not
configured status rather than preventing the HUD from running.

### Docker

Docker monitoring performs one bounded command:

```text
docker ps --all --format "{{.ID}}\t{{.Names}}\t{{.State}}\t{{.Status}}"
```

It does not run `docker inspect` once per container. Docker collection has its
own worker thread, output limit, timeout, retry backoff, and last-good snapshot.
The compact HUD reports a summary; selecting it reveals affected containers.

### Kubernetes

Kubernetes monitoring performs a bounded `kubectl get pods,deployments -A -o
json` collection in another worker. It recognizes selected pod conditions such
as `CrashLoopBackOff`, image-pull failures, `OOMKilled`, and container errors,
as well as deployments whose available replica count is below the desired count.

The command uses your active `kubectl` context. Treat it as a cluster-wide
summary, and ensure that context and permissions are appropriate before enabling
the overlay in a production environment.

## Configuration

Configuration is optional. Defaults are compiled in; missing or invalid values
fall back to validated defaults.

Default path:

```text
~/.config/debug-overlay/config.ini
```

Override it with `DEBUG_OVERLAY_CONFIG=/path/to/config.ini`.

```ini
[health]
cpu_warn_percent  = 70
cpu_high_percent  = 90
rss_warn_kb       = 1048576
sustained_samples = 3

[overlay]
sample_interval_ms = 1000
focus_cache_ttl_ms = 1500

[docker]
healthy_ms       = 20000
issue_ms         = 8000
backoff_start_ms = 15000
backoff_cap_ms   = 240000
absent_ms        = 300000
timeout_ms       = 3000

[kubernetes]
healthy_ms       = 30000
issue_ms         = 10000
backoff_start_ms = 15000
backoff_cap_ms   = 240000
absent_ms        = 300000
timeout_ms       = 5000
```

Useful diagnostics:

- `DEBUG_OVERLAY_PROBE=1` prints the chosen focus provider and exits.
- `DEBUG_OVERLAY_LOG=/path/to/file` appends lifecycle/source summaries.
- `DEBUG_OVERLAY_LATENCY=1` writes GUI-timer and focus-update timing data to
  standard error.

## Architecture

```text
                       Qt GUI thread
              HudWindow: input and rendering only
                              ▲
               typed queued snapshots
              ┌───────────────┼────────────────┐
              │               │                │
    sampler worker       Docker worker      Kubernetes worker
    focus → group →      one QProcess,      one QProcess,
    CPU + RSS            deadline/backoff   deadline/backoff
```

- The GUI thread does not scan `/proc` or wait for Docker/Kubernetes commands.
- The focused-app sampler caches a process group and refreshes it when necessary
  instead of discovering the full tree for every display update.
- Docker and Kubernetes each permit one command at a time. A slow or unavailable
  source cannot queue unlimited work or delay CPU/RSS updates.
- Source results cross threads as typed snapshots. Last-known-good Docker/Kubernetes
  results are marked stale when a subsequent query fails.

## Testing

The CMake project registers 12 test executables covering the HUD controls,
focus providers, `/proc` parsing, CPU calculation, memory formatting, process
grouping, health state, configuration, Docker, Kubernetes, and adapter failure
paths.

```bash
ctest --test-dir cpp/build --output-on-failure
```

`cpp/tools/bench.sh` is a small process-level benchmark helper. Use it to make
your own repeatable measurements rather than relying on unverified headline
numbers:

```bash
# Measure an already-running overlay process.
bash cpp/tools/bench.sh --pid "$(pgrep -n debug_overlay_hud)" --duration 60
```

For a fair Python-vs-Qt comparison, use the same desktop session, focused app,
enabled integrations, sample duration, and power profile. Record CPU, RSS/PSS,
wakeups, thread count, and focus-to-display latency.

## Project layout

```text
cpp/
├── src/                 Qt HUD, samplers, focus adapters, Docker/Kubernetes sources
├── tests/               CTest executables
├── tools/bench.sh       process benchmark helper
├── packaging/           Debian builder, RPM spec, Arch PKGBUILD, integration notes
├── install.sh           per-user install, autostart, and uninstall
└── kwin_script/         KDE Plasma focus bridge
gnome_extension/         GNOME focus bridge
overlay_design.py        original Tkinter implementation
```

## Current status

Implemented: focused-app CPU/RSS HUD, supported focus adapters, compact and
details UI states, Docker/Kubernetes sources, configuration, tests, per-user
installation, and packaging templates.

Before publishing performance claims, run the benchmark on the target desktop
and record the environment, sample duration, active integrations, and results.

## License

No license file is currently included. Until a license is added, the repository
is not offered under an open-source license.
