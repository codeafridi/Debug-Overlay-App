# Debug Overlay

**A lightweight, always-on-top HUD that shows the health of the app you're actually using — and your Docker/Kubernetes infra — without opening a terminal.**

[![Platform: Linux](https://img.shields.io/badge/platform-Linux-informational?logo=linux&logoColor=white)](#)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](#)
[![Qt 6](https://img.shields.io/badge/Qt-6-green?logo=qt&logoColor=white)](#)
[![Tests](https://img.shields.io/badge/tests-12%20passing-brightgreen)](#testing)
[![Distros](https://img.shields.io/badge/distros-Ubuntu%20%7C%20Debian%20%7C%20Fedora%20%7C%20Arch%20%7C%20openSUSE-orange)](#supported-distributions)

> Repository name: **Syspeek** · Rewritten in **C++20 / Qt 6** for low CPU, low RAM, and a responsive GUI.

```
 ┌ Debug Overlay ──────────────────────────────── OK ┐
 │  App            PID          CPU        MEM (RSS)│
 │  cursor         2092707      Warming up 212 MB   │
 │  docker: 1 running                               │
 │  k8s: 4 issues                                   │
 │                                  MORE   HIDE     │
 └──────────────────────────────────────────────────┘
```

---

## Index

- [Why](#why)
- [Features](#features)
- [Screenshots](#screenshots)
- [Supported distributions](#supported-distributions)
- [Supported desktops](#supported-desktops-focus-detection)
- [Installation](#installation)
- [Build from source](#build-from-source)
- [Usage](#usage)
- [Configuration](#configuration)
- [Docker & Kubernetes](#docker--kubernetes)
- [Performance](#performance)
- [Architecture](#architecture)
- [Testing](#testing)
- [Project layout](#project-layout)
- [Status / roadmap](#status--roadmap)
- [License](#license)

---

## Why

`top`/`htop`/`btop` show *every* process and expect you to find the one you care
about. Debug Overlay flips that: it watches **the window you're focused on** and
reports that app's health, resolved to a stable process identity and **grouped
with its helper processes** (browsers, Electron IDEs, games) so its CPU and RAM
numbers actually mean something.

It's a small glanceable panel you leave on screen while you work — no terminal,
no hunting through PIDs.

## Features

- **Focused-app aware** — the app, PID and process group you're currently using.
- **Whole-app metrics** — CPU and RSS aggregated across the app + its helpers.
- **Compact health state** — `OK` / `WARN` / `HIGH CPU` from *sustained* thresholds (no jittery one-sample alerts).
- **Optional Docker badge** — running / unhealthy / restarting / newly-exited containers.
- **Optional Kubernetes badge** — pod and deployment issues (CrashLoopBackOff, ImagePullBackOff, OOMKilled, unavailable replicas).
- **On-demand details** — click a badge or `MORE`; the overlay never grows by itself.
- **Non-invasive UI** — frameless, always-on-top, never steals focus.
- **Configurable** — thresholds and intervals via a simple INI file.
- **Runs everywhere, degrades gracefully** — missing tools / daemons / unsupported desktops produce a clear status instead of a crash.
- **Tiny footprint** — see [Performance](#performance).

## Screenshots

> Add screenshots/GIFs here (e.g. `docs/hud.png`). The compact state, the
> expanded diagnosis panel, and a Docker/Kubernetes badge in the error state are
> the most useful shots.

---

## Supported distributions

Build **and** full test suite verified in clean containers:

| Distribution | Qt 6 | Build | Tests | Launch |
|---|---|:---:|:---:|:---:|
| Ubuntu 26.04 | 6.10 | ✅ | ✅ 12/12 | ✅ |
| Debian 13 | 6.8 | ✅ | ✅ 12/12 | ✅ |
| Fedora 44 | 6.11 | ✅ | ✅ 12/12 | ✅ |
| Arch Linux | 6.12 | ✅ | ✅ 12/12 | ✅ |
| openSUSE Tumbleweed | 6.11 | ✅ | ✅ 12/12 | ✅ |

Linux Mint (Ubuntu base) and CachyOS (Arch base) are covered by the same paths.
The monitoring core uses only Qt 6 + `/proc`, so any mainstream Linux works.

## Supported desktops (focus detection)

Debug Overlay is PID-centric. Wayland has **no standard API for the focused PID**,
so each desktop supplies it through its own channel:

| Desktop | Mechanism | Status |
|---|---|---|
| GNOME (Wayland & X11) | GNOME Shell extension | ✅ verified |
| X11 / XWayland (Cinnamon, XFCE, MATE, KDE-X11) | `xdotool` | ✅ verified |
| Sway | `swaymsg` | ✅ verified (real compositor) |
| niri | `niri msg` | ✅ verified (real compositor) |
| KDE Plasma Wayland | KWin script + D-Bus | ⚠️ implemented, unverified |
| Hyprland | `hyprctl` | ⚠️ implemented, unverified |
| Other Wayland (COSMIC, river, labwc, …) | — | ❌ no adapter → runs, shows `no-focus` |

Check yours in one command (no GUI needed):

```bash
DEBUG_OVERLAY_PROBE=1 ./cpp/build/debug_overlay_hud
# provider=swaymsg
# focused_pid=1313
# focused_comm=foot
```

## Installation

**From source (any distro):**

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"

# install + autostart (systemd user service, or XDG autostart fallback)
bash cpp/install.sh
# remove everything
bash cpp/install.sh --uninstall
```

**Debian/Ubuntu package:**

```bash
bash cpp/packaging/build_deb.sh       # -> cpp/build/packaging/debug-overlay_*_*.deb
sudo apt install ./cpp/build/packaging/debug-overlay_*_*.deb
```

RPM spec (`cpp/packaging/rpm/`) and Arch `PKGBUILD` (`cpp/packaging/arch/`) are
provided as templates (build on those distros).

**GNOME users** also install the focus bridge once, then log out/in:

```bash
bash install_gnome_extension.sh
```

## Build from source

**Prerequisites**

| Distribution | Packages |
|---|---|
| Debian / Ubuntu / Mint | `build-essential cmake qt6-base-dev` |
| Fedora | `gcc-c++ cmake qt6-qtbase-devel` |
| Arch / CachyOS | `base-devel cmake qt6-base` |
| openSUSE | `gcc-c++ cmake qt6-base-devel` |

Optional: `xdotool` (X11 focus), `docker` (Docker badge), `kubectl` (Kubernetes badge).

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
ctest --test-dir cpp/build --output-on-failure
```

## Usage

Launch `./cpp/build/debug_overlay_hud` (or the installed `debug-overlay`).
Then:

- **Drag** it anywhere — or grab the buttons; it moves the whole panel.
- **`MORE` / `LESS`** — toggle the diagnosis panel (memory note + issue details).
- **`HIDE` / `SHOW`** — collapse to a single summary line.
- **Click a badge** (`docker:` / `k8s:`) to expand its details.
- **Ctrl+C** in the terminal to quit.

## Configuration

Optional INI file at `~/.config/debug-overlay/config.ini`
(`DEBUG_OVERLAY_CONFIG` overrides the path). Missing file or invalid values fall
back to safe defaults.

```ini
[health]
cpu_warn_percent  = 70
cpu_high_percent  = 90
rss_warn_kb       = 1048576
sustained_samples = 3

[docker]
healthy_ms       = 20000
issue_ms         = 8000
backoff_start_ms = 15000
backoff_cap_ms   = 240000
timeout_ms       = 3000

[kubernetes]
healthy_ms = 30000
issue_ms   = 10000
timeout_ms = 5000

[overlay]
sample_interval_ms = 1000
focus_cache_ttl_ms = 1500
```

Diagnostics (env):

- `DEBUG_OVERLAY_LOG=<path>` — append structured status/lifecycle logging.
- `DEBUG_OVERLAY_PROBE=1` — print the selected focus provider + focused PID and exit.
- `DEBUG_OVERLAY_LATENCY=1` — print GUI-jitter / focus-update latency to stderr.

## Docker & Kubernetes

Both are **optional, isolated** sources on their own worker threads.

- **Docker:** one bounded `docker ps --all` call (no per-container `inspect`).
  Distinguishes *not installed / daemon unreachable / permission denied / N
  running / M unhealthy*, and flags a container that goes **running → exited
  non-zero** as recently crashed.
- **Kubernetes:** one bounded `kubectl get pods,deployments -A -o json` call.
  Flags `CrashLoopBackOff`, `ImagePullBackOff`/`ErrImagePull`,
  `OOMKilled`/`Error`, failed/unknown phases, high restart counts, and
  deployments with unavailable replicas.

Both back off on failure and keep the last good result marked **stale**.

## Performance

Reference machine: Ubuntu 26.04, GNOME Wayland, 12 cores. Idle, focused = Cursor
(median of 5 × 30 s). Python = the original Tkinter reference implementation.

| Metric | Python (Tk) | C++ / Qt 6 |
|---|---|---|
| CPU (% of one core) | 26.2 % | **0.67 %** |
| Voluntary wakeups/s | ~4,600 | **~2.1** |
| RSS | ~62 MB | ~60 MB |
| PSS | ~51 MB | **~37.5 MB** |

- Focus-update latency: median **20 ms**.
- Active load (focused app at ~100 %): overlay's own CPU **0.20 %**.
- 10-minute soak: **0.74 % CPU**, memory flat.

## Architecture

<details>
<summary>Design overview</summary>

```

            ┌──────────────── GUI THREAD (Qt) ────────────────┐
            │  HudWindow — render-only; never scans /proc     │
            └───────▲───────────────────────────────┬─────────┘
                    │ queued snapshots               │ file watch
        ┌───────────┴────────────┐                   │
        │  SamplerWorker (thread)│                   │
        │  focus→identity→group  │                   │
        │  CPU + RSS             │                   │
        └───────────┬────────────┘                   │
                    │ FocusedWindowProvider adapters  │
        ┌───────────┴──────┐   ┌───────────┴──────────┐
        │ DockerSource     │   │ KubeSource           │
        │ (thread, bounded)│   │ (thread, bounded)    │
        └──────────────────┘   └──────────────────────┘
```

- **Render-only GUI** — the GUI thread never blocks on monitoring.
- **Bounded workers** — one thread per source, at most one query in flight, no
  queued history, deadlines + exponential backoff, safe shutdown.
- **Value snapshots** — results cross threads as typed values; stale/out-of-order
  results are rejected.
- **Isolated slow sources** — Docker/Kubernetes can never delay focus/CPU/RSS.
</details>

## Testing

`ctest` runs 12 suites (~all parsing/logic, no GUI): `/proc` parsing, CPU
sampling, RSS, process grouping, snapshots, health thresholds, provider
selection/caching, config validation, Docker, Kubernetes, and the focus adapters
(via fake commands, including timeout/error paths).

```bash
ctest --test-dir cpp/build --output-on-failure
```

## Project layout

```
cpp/
├── CMakeLists.txt
├── src/                 core library + app (sampler, sources, GUI, config)
├── tests/               unit tests (ctest)
├── tools/bench.sh       process benchmark harness
├── packaging/           .deb builder, RPM spec, Arch PKGBUILD, docs
├── install.sh           install / uninstall + autostart
├── install_kde_focus.sh KDE focus bridge installer
└── kwin_script/         KWin focus bridge
gnome_extension/         GNOME Shell focus bridge
overlay_design.py        original Python/Tk reference implementation
```

## Status / roadmap

- ✅ Focused-app HUD, grouping, CPU/RSS, health status.
- ✅ Docker + Kubernetes badges with on-demand details.
- ✅ Worker-thread architecture, config, install, `.deb`, cross-distro builds.
- 🔜 More desktop adapters (the app supports any compositor that exposes a PID),
  deeper Kubernetes detail, notifications.

## License

No license file is included yet. Until one is added, all rights are reserved by
the author. Add a `LICENSE` before distributing.
