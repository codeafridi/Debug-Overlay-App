<div align="center">

# Debug Overlay

### A tiny, always-on-top Qt 6 HUD for the focused Linux application — and your Docker/Kubernetes infra.

[![Platform: Linux](https://img.shields.io/badge/platform-Linux-informational?style=flat-square&logo=linux&logoColor=white)](#requirements)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)](#build-from-source)
[![Qt 6](https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt&logoColor=white)](#requirements)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)](#build-from-source)

[Quick start](#quick-start) · [Features](#features) · [Focus detection](#focus-detection) · [Docker & Kubernetes](#docker--kubernetes) · [Configuration](#configuration) · [Architecture](#architecture) · [Testing](#testing)

<sub>Repository: <strong>Syspeek</strong></sub>

</div>

---

```text
┌ DEBUG HUD ──────────────────────────────────────────────────── OK ┐
│ App            PID          CPU          MEM (RSS)               │
│ cursor         2092707      12%          212 MB                  │
│ docker: 1 running                                                │
│ k8s: 4 issues                                    MORE   HIDE     │
└──────────────────────────────────────────────────────────────────┘
```

Debug Overlay is a small, always-on-top Linux desktop monitor. Instead of making
you hunt through every process in `top`, it follows the window you are currently
using and shows a compact view of that application's CPU and resident memory. It
can also surface optional Docker and Kubernetes status — kept off the UI thread so
a slow or unavailable integration never freezes the HUD.

It builds and runs on any mainstream Linux desktop with Qt 6 and `/proc`.

## Features

- **Focused-app monitoring** — application name, PID, CPU, and RSS.
- **Whole-app metrics** — device processes are grouped with their helpers, and a
  start-time identity guards against reused PIDs.
- **Compact health state** — `OK` / `WARN` / `HIGH CPU` from sustained samples,
  not single-sample noise.
- **Non-invasive UI** — frameless, always-on-top, and it does not take keyboard
  focus.
- **On-demand detail** — `MORE` opens the diagnosis panel; `HIDE` collapses to a
  single line; Docker/Kubernetes badges expand when selected.
- **Optional Docker** — running count plus unhealthy, restarting, and recently
  exited containers.
- **Optional Kubernetes** — pod failure reasons and unavailable deployment
  replicas.
- **Isolated slow sources** — Docker and Kubernetes run on their own worker
  threads with bounded output, deadlines, retry backoff, and stale-result
  handling.
- **Validated configuration** — one INI file for thresholds and polling.
- **Per-user install** — a systemd user service where available, with an XDG
  autostart fallback.

## What it is not

This is deliberately not a replacement for a full dashboard, `htop`, Docker
Desktop, or a Kubernetes observability stack. The normal view stays small and
glanceable.

CPU and RSS describe the focused application. Docker and Kubernetes values are
separate infrastructure signals and are not attributed to the focused app. RSS is
summed across the selected process group, so shared memory can be counted more
than once.

## Quick start

Build and run the development binary:

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
ctest --test-dir cpp/build --output-on-failure

./cpp/build/debug_overlay_hud
```

Install for your user (adds a startup entry):

```bash
bash cpp/install.sh              # systemd user service when available
bash cpp/install.sh --no-systemd # XDG autostart instead
bash cpp/install.sh --uninstall  # remove everything
```

Check focus detection without opening the HUD:

```bash
DEBUG_OVERLAY_PROBE=1 ./cpp/build/debug_overlay_hud
# provider=gnome-pidfile
# focused_pid=2092707
# focused_comm=cursor
```

## Requirements

- A Linux desktop with `/proc`
- CMake 3.16 or newer
- A C++20 compiler
- Qt 6 Widgets and Qt 6 DBus development packages

Typical package names:

| Distribution | Packages |
| --- | --- |
| Debian / Ubuntu / Mint | `build-essential cmake qt6-base-dev` |
| Fedora / RHEL | `gcc-c++ cmake qt6-qtbase-devel` |
| Arch / CachyOS | `base-devel cmake qt6-base` |
| openSUSE | `gcc-c++ cmake qt6-base-devel` |

Optional integrations: `xdotool` (X11 focus), `docker` (Docker status), and
`kubectl` with an accessible current context (Kubernetes status).

## Install

The project ships a per-user installer, a Debian package builder, and RPM /
Arch packaging templates. See [`cpp/packaging/README.md`](cpp/packaging/README.md).

```bash
bash cpp/packaging/build_deb.sh   # -> cpp/build/packaging/debug-overlay_*_*.deb
```

## Focus detection

Wayland has no single standard API that exposes the focused window's PID, so
Debug Overlay selects an adapter from your session and still runs everywhere —
compositors without an adapter simply show `no-focus`.

| Desktop / session | Adapter | Extra setup |
| --- | --- | --- |
| GNOME | GNOME Shell PID-file bridge | Install the bridge, then log out/in |
| KDE Plasma Wayland | KWin script + D-Bus bridge | `bash cpp/install_kde_focus.sh` |
| Hyprland | `hyprctl` | `hyprctl` available |
| Sway | `swaymsg` | `swaymsg` available |
| niri | `niri msg` | `niri` available |
| X11 / XWayland (Cinnamon, XFCE, MATE, …) | `xdotool` | `xdotool` installed |
| Other Wayland compositors | — | HUD runs with `no-focus` |

Install the GNOME bridge once:

```bash
bash install_gnome_extension.sh   # then log out and back in
```

## Docker & Kubernetes

Both sources are optional; a missing tool appears as an unavailable/not-configured
status rather than stopping the HUD.

**Docker** performs one bounded command and never runs `docker inspect` per
container:

```text
docker ps --all --format "{{.ID}}\t{{.Names}}\t{{.State}}\t{{.Status}}"
```

**Kubernetes** performs one bounded collection using your active `kubectl`
context:

```text
kubectl get pods,deployments -A -o json
```

It recognizes `CrashLoopBackOff`, image-pull failures, `OOMKilled`, and container
errors, plus deployments whose available replicas fall below the desired count.
Treat it as a cluster-wide summary and ensure the context and permissions are
appropriate before enabling it in a production environment.

## Configuration

Configuration is optional; missing or invalid values fall back to validated
defaults at `~/.config/debug-overlay/config.ini` (override with
`DEBUG_OVERLAY_CONFIG`).

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

Diagnostics (environment variables):

- `DEBUG_OVERLAY_PROBE=1` prints the chosen focus provider and exits.
- `DEBUG_OVERLAY_LOG=/path/to/file` appends lifecycle/source summaries.
- `DEBUG_OVERLAY_LATENCY=1` writes GUI-timer and focus-update timing to stderr.

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

- The GUI thread never scans `/proc` or waits on Docker/Kubernetes commands.
- The focused-app sampler caches a process group and refreshes it only when
  needed, instead of rediscovering the full tree every update.
- Each source allows one command at a time, so a slow or unavailable integration
  cannot queue unbounded work or delay CPU/RSS updates.
- Results cross threads as typed snapshots; last-known-good Docker/Kubernetes
  results are marked stale when a later query fails.

## Testing

The CMake project registers 12 test executables covering the HUD controls, focus
providers, `/proc` parsing, CPU calculation, memory formatting, process grouping,
health state, configuration, Docker, Kubernetes, and adapter failure paths.

```bash
ctest --test-dir cpp/build --output-on-failure
```

`cpp/tools/bench.sh` is a small process-level benchmark helper. Measure your own
environment rather than relying on headline numbers, and for any comparison use the
same session, focused app, enabled integrations, duration, and power profile:

```bash
bash cpp/tools/bench.sh --pid "$(pgrep -n debug_overlay_hud)" --duration 60
```

## Project layout

```text
cpp/
├── src/                 Qt HUD, samplers, focus adapters, Docker/Kubernetes sources
├── tests/               CTest executables
├── tools/bench.sh       process benchmark helper
├── packaging/           Debian builder, RPM spec, Arch PKGBUILD, integration notes
├── install.sh           per-user install, autostart, and uninstall
├── install_kde_focus.sh KDE focus bridge installer
└── kwin_script/         KDE Plasma focus bridge
gnome_extension/         GNOME focus bridge
install_gnome_extension.sh
```

## Status

Implemented: focused-app CPU/RSS HUD, the focus adapters above, compact and
details UI states, Docker/Kubernetes sources, configuration, per-user
installation, and packaging templates.

Planned: additional desktop adapters (any compositor exposing a focused PID can
be supported), deeper Kubernetes detail, and notifications.

## Contributing

Issues and pull requests are welcome. Keep the monitoring core
distribution-independent and desktop-specific behavior behind the focus-adapter
interface.

## License

No license file is included yet. Until one is added, the repository is not offered
under an open-source license.
