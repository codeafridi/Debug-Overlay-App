# Debug Overlay

**A lightweight, always-on-top HUD for live process and infrastructure status on Linux desktops.**

Debug Overlay watches the application you are actually using and shows, at a glance,
the focused app, its PID, CPU, and memory — plus optional Docker and Kubernetes
health — without leaving your workflow or opening a terminal.

> Rewritten in C++20 / Qt 6 for low CPU, low memory, and a responsive GUI.
> Designed to monitor *apps with helper processes* (browsers, Electron IDEs, games)
> as a single unit, not one PID.

```
 Debug Overlay                                OK
 App            PID        CPU       MEM (RSS)
 cursor         2092707    Warming up  212 MB
 docker: 1 running
 k8s: 4 issues
                              MORE   HIDE
```

---

## Highlights

- **Focused-app aware** — shows the app you are using, resolved to a stable
  **process identity** (PID + start time) so PID reuse can't fool it.
- **Whole-app metrics** — groups the app with its helper processes and reports
  **aggregate CPU and RSS**, so browser/Electron numbers are meaningful.
- **Compact health state** — `OK` / `WARN` / `HIGH CPU` from sustained CPU/RSS
  thresholds (no noisy one-sample alerts).
- **Optional Docker & Kubernetes** — container and pod health as small, clickable
  badges; details only on demand.
- **Non-invasive UI** — a frameless, always-on-top overlay that never steals focus
  and never queues stale samples.
- **Graceful by design** — missing tools, unreachable daemons, and unsupported
  desktops degrade to a clear status instead of breaking.
- **Broad Linux support** — X11 and major Wayland compositors via per-desktop
  adapters; the monitoring core is distribution-independent.

---

## What it shows

| Field | Meaning |
|---|---|
| **App** | Focused application name (from `/proc`) |
| **PID** | Focused process ID |
| **CPU** | CPU use of the *app group*, in % of one logical core (multithreaded apps can exceed 100%) |
| **MEM (RSS)** | Resident memory **summed** across the app group (shared pages may be counted more than once) |
| **Status** | `OK`, `WARN`, or `HIGH CPU` from sustained thresholds |
| **docker** | Optional: `N running`, `N running \| M issue(s)`, or a clear error state |
| **k8s** | Optional: `N pods`, `N issues`, or a clear error state |

Click a **docker**/**k8s** badge for details; press **MORE** for a combined
diagnosis panel; press **HIDE** to collapse to a single line. Drag it anywhere.

---

## Architecture

A deliberately small, testable design — every unit has one job.

```
                 ┌───────────────────────────────────────────┐
                 │              GUI THREAD (Qt)              │
                 │   HudWindow — render-only, no monitoring  │
                 └───────────────▲───────────────┬───────────┘
                                 │ queued        │ file-watch
                                 │ snapshots     │ (GNOME focus)
        ┌────────────────────────┴───────┐       │
        │   SamplerWorker (QThread)      │       │
        │   focus → identity → group     │       │
        │   CPU + RSS snapshots          │       │
        └───────────────┬────────────────┘       │
                        │ FocusedWindowProvider   │
                        │ adapters (GNOME/KDE/…)  │
        ┌───────────────┴──────┐   ┌──────────────┴───────┐
        │ DockerSource (thread)│   │  KubeSource (thread) │
        │ bounded async poll   │   │  bounded async poll  │
        └──────────────────────┘   └──────────────────────┘
```

- **Render-only GUI.** The GUI thread never scans `/proc`, runs subprocesses, or
  waits on workers. It renders the newest completed `DisplaySnapshot`.
- **Bounded workers.** One long-lived thread per source; at most one query in
  flight, no queued history, deadlines + exponential backoff, safe shutdown.
- **Value snapshots.** Results cross threads as typed values via queued signals;
  stale/out-of-order snapshots are rejected.
- **Isolated slow sources.** Docker and Kubernetes run on separate threads with
  timeouts, output caps, and last-known-good/stale marking, so they can never
  delay the critical focus/CPU/RSS path.

---

## Supported desktops (focus detection)

Debug Overlay is PID-centric. Wayland has no generic way to obtain the focused
PID (the foreign-toplevel protocols expose only `app_id`/`title`), so each desktop
supplies it through its own channel:

| Session | Mechanism | Status |
|---|---|---|
| GNOME (Wayland & X11) | GNOME Shell extension (`gnome_extension/`) | implemented; verified on Ubuntu GNOME Wayland |
| KDE Plasma (Wayland) | KWin script + D-Bus (`cpp/kwin_script`, `cpp/install_kde_focus.sh`) | implemented; **not yet verified** on KDE |
| Hyprland | `hyprctl` | implemented; **not yet verified** on a real session |
| Sway | `swaymsg` | implemented; **verified** against a real (headless) Sway compositor |
| niri | `niri msg` | implemented; **verified** against a real (nested) niri compositor |
| X11 / XWayland (Cinnamon, XFCE, MATE, KDE-X11) | `xdotool` | implemented; **verified** against a real (headless Xvfb) X server |
| Other Wayland compositors | — | not supported; shows `no-focus` |

The provider is auto-selected from `XDG_SESSION_TYPE`/`XDG_CURRENT_DESKTOP`.
*These are compatibility targets, not universal support claims.*

**Verify focus detection on your desktop in one command** (no GUI needed):

```bash
DEBUG_OVERLAY_PROBE=1 ./cpp/build/debug_overlay_hud
# provider=swaymsg
# focused_pid=1313
# focused_comm=foot
```

It prints the selected provider and the focused PID (or `focused_pid=none`).
For KDE it waits briefly for the KWin script's D-Bus push.

---

## Build

**Prerequisites**

| Distribution | Packages |
|---|---|
| Debian / Ubuntu / Mint | `build-essential cmake qt6-base-dev libqt6dbus6` |
| Fedora | `gcc-c++ cmake qt6-qtbase-devel qt6-qtbase` |
| Arch / CachyOS | `base-devel cmake qt6-base` |

Plus `xdotool` for X11 sessions, and a GNOME/KDE focus script on those desktops
(see above).

**Compile and test**

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j"$(nproc)"
ctest --test-dir cpp/build --output-on-failure
```

Run it:

```bash
./cpp/build/debug_overlay_hud
```

**Verified builds** (configure + compile + all tests pass, in clean containers):

| Distribution | Qt 6 | Result |
| --- | --- | --- |
| Ubuntu 26.04 (dev machine) | 6.10.2 | ✅ build + 12/12 tests |
| Debian 13 | 6.8.2 | ✅ build + 12/12 tests |
| Fedora 44 | 6.11.2 | ✅ build + 12/12 tests |
| Arch Linux | 6.12.0 | ✅ build + 12/12 tests |
| openSUSE Tumbleweed | 6.11.2 | ✅ build + 12/12 tests |

(Linux Mint and CachyOS share the Ubuntu/Arch bases respectively.) This validates
the build and dependency assumptions across Qt 6.8–6.12; it does not, by itself,
validate on-desktop focus integration on each distro.

---

## Install (autostart at login)

```bash
bash cpp/install.sh                 # systemd user service if available
bash cpp/install.sh --no-systemd    # XDG autostart only
bash cpp/install.sh --uninstall
```

Installs the binary to `~/.local/bin/debug-overlay` (`XDG_BINDIR` overrides),
uses a **systemd user service** where present, and falls back to an
**XDG autostart** entry otherwise.

GNOME users also need the focus bridge once:

```bash
bash install_gnome_extension.sh     # then log out and back in
```

---

## Configuration

Optional INI file at `~/.config/debug-overlay/config.ini`
(override with `DEBUG_OVERLAY_CONFIG`). Missing file or invalid values fall back
to safe defaults.

```ini
[health]
cpu_warn_percent = 70
cpu_high_percent = 90
rss_warn_kb      = 1048576
sustained_samples = 3

[docker]
healthy_ms      = 20000
issue_ms        = 8000
backoff_start_ms = 15000
backoff_cap_ms  = 240000
timeout_ms      = 3000

[kubernetes]
healthy_ms      = 30000
issue_ms        = 10000
timeout_ms      = 5000

[overlay]
sample_interval_ms = 1000
focus_cache_ttl_ms = 1500
```

Set `DEBUG_OVERLAY_LOG=<path>` to append structured status/lifecycle logging to a
file.

---

## Docker & Kubernetes

- **Docker:** one bounded `docker ps --all --format …` call (no per-container
  inspect). Distinguishes *not installed*, *daemon unreachable*,
  *permission denied*, and *M running / N unhealthy/restarting*. Compact badge +
  on-demand details.
- **Kubernetes:** one bounded `kubectl get pods -A -o json` call. Flags
  `CrashLoopBackOff`, `ImagePullBackOff`/`ErrImagePull`, `OOMKilled`/`Error`,
  failed/unknown phases, and high restart counts. Compact badge + on-demand
  details. Unreachable clusters show `k8s: unreachable`.
- Both poll on a backoff schedule and keep the last good result marked **stale**
  when a later poll fails.

---

## Performance

Measured on the reference machine (Ubuntu 26.04, GNOME Wayland, 12 logical cores),
idle, focused app = Cursor (median of 5 × 30 s runs):

| Metric | Python (Tkinter) | C++ / Qt 6 |
|---|---|---|
| CPU (% of one core) | 26.2 % | **0.67 %** (0.30–0.70) |
| Voluntary wakeups/s | ~4,600 | **~2.1** |
| RSS | ~62 MB | ~60.1 MB |
| PSS | ~51 MB | **~37.5 MB** |

Responsiveness and endurance (C++):

- **Focus-update latency** (focus change → snapshot applied): median **20 ms**
  (min 18, max 29; n = 12).
- **GUI event-loop jitter**: no late timer ticks observed while monitoring.
- **Active load** (focused app pegged at ~100 % CPU): overlay's own CPU **0.20 %**.
- **10-minute soak**: **0.74 % CPU**, memory flat (RSS +0.3 MB, PSS +0.1 MB).

The design targets: no blocking monitoring on the GUI thread, no unbounded
queues, no fast idle polling, and correct stale/unavailable states.

Instrumentation is opt-in: set `DEBUG_OVERLAY_LATENCY=1` to print GUI-jitter and
focus-update latency to stderr (off by default, so it costs nothing normally).

---

## Testing

`ctest` covers parsing and logic without a GUI:
focus/identity, CPU sampling, RSS formatting, process grouping, snapshots,
health thresholds, provider selection/caching, Docker parsing/states, Kubernetes
parsing/states, and configuration loading/validation.

---

## Project layout

```
cpp/
├── CMakeLists.txt
├── src/                 core library + app (sampler, sources, GUI, config)
├── tests/               unit tests (ctest)
├── tools/bench.sh       process benchmark harness
├── packaging/           systemd unit, autostart entry, docs
├── install.sh           install / uninstall
├── install_kde_focus.sh KDE focus bridge installer
└── kwin_script/         KWin focus bridge
gnome_extension/         GNOME Shell focus bridge
overlay_design.py        original Python/Tk reference implementation
```

The original Python application is kept as the **reference implementation and
benchmark baseline**.

---

## Compatibility

Targets mainstream desktop Linux (Ubuntu, Linux Mint, Debian, Fedora, Arch,
CachyOS, openSUSE). The monitoring core uses only `/proc` and standard kernel
interfaces; only focus detection and autostart are desktop-specific.
