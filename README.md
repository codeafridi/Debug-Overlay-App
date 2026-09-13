# Debug Overlay

Small Linux debug overlay for a focused or selected app.

It stays on top of the screen, stays compact when things are normal, and expands into a diagnosis panel when it catches something odd like high CPU, rising memory, disk pressure, or log/network signals.

## What this project does

This overlay watches the currently focused app and shows:

- app name
- focused PID
- app-level CPU usage
- app-level RSS memory
- short diagnosis when an alert is triggered

For apps like Firefox, Chrome, VS Code, and other multi-process apps, the overlay does not rely only on one child PID. It tries to group related processes so the numbers are more useful.

<img width="1154" height="774" alt="image" src="https://github.com/user-attachments/assets/1a6cc9d0-1164-48ba-831e-abed1bbc1701" />


## Before you start

This project is meant for Linux desktop sessions. It supports Ubuntu, Debian,
Linux Mint, Fedora, Red Hat Enterprise Linux, Arch, and other distributions
that provide Python, Tk, and the Linux `/proc` filesystem.

You need:

- Python 3
- Tkinter for Python
- `xdotool` (for automatic tracking in X11 sessions)

The overlay uses:

- `/proc` for process information
- `xdotool` to detect the active window PID on X11
- `journalctl` for recent log-based alerts

## Desktop-session support

Linux distributions share the same process interfaces, but desktop sessions do
not share one universal API for reading the focused window. The overlay uses
the best supported mode for the current session:

| Session | Tracking mode |
| --- | --- |
| X11 / Xorg | Automatic focused-window tracking through `xdotool` |
| Hyprland on Wayland | Automatic focused-window tracking through `hyprctl` |
| GNOME Wayland, KDE Wayland, Sway, and other Wayland compositors | Select a running process with the `TARGET` button |

The `TARGET` mode monitors the chosen process and its related child processes.
It works regardless of the Linux distribution or Wayland compositor. Select a
new target whenever you want to inspect a different app.

Wayland deliberately prevents ordinary applications from reading the focused
window across all apps. Automatic focus tracking on a Wayland desktop therefore
requires a compositor-specific integration; it cannot be supplied by `xdotool`.

If `journalctl` is not available, the overlay will still run, but log-based alerts may not work properly.

## Install step by step

### 1. Clone the repo

```bash
git clone https://github.com/codeafridi/Debug-Overlay-App.git
cd Debug-overlay
```

### 2. Install system packages

On Ubuntu / Debian:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-tk xdotool
```

On Arch:

```bash
sudo pacman -S python tk xdotool
```

On Fedora:

```bash
sudo dnf install python3 python3-tkinter xdotool
```

If you already have Python and Tkinter working, the only thing people usually miss is `xdotool`.

### 3. Create a virtual environment

From the project folder:

```bash
python3 -m venv venv
```

### 4. Activate it

```bash
source venv/bin/activate
```

### 5. Install Python packages if needed

Right now this project mainly uses the standard library, so there is not much to install through `pip`.

If later you add more dependencies, this is where users would install them.

## How to run it

You do not need to start a separate server for the current version.

Just run:

```bash
./start_overlay.sh
```

That script:

- uses `xdotool` automatically when it is available on X11
- uses the local virtual environment if it exists
- starts `overlay_design.py`

If the script is not executable on your machine, run:

```bash
chmod +x start_overlay.sh
./start_overlay.sh
```

## What the overlay shows

### Compact transparent bar

When nothing important is happening, the overlay stays in a compact transparent state.

You will usually see:

- app name
- focused PID
- CPU
- RSS memory
- current state like `STABLE`

### Expanded diagnosis panel

When something is detected, the overlay expands and shows diagnosis details.

This can include:

- CPU watch
- memory watch
- disk watch
- network watch
- log alert
- app event / crash-like change

## Basic controls

- drag the overlay to move it
- `TARGET` selects the PID to monitor; this is required on unsupported Wayland desktops
- `FRZ` freezes the live updates
- `MORE` expands the details panel

To find an application PID, run this in a terminal, then enter the PID shown in
the first column:

```bash
ps -e -o pid,comm | less
```

When frozen:

- the overlay stops updating
- you can inspect the current diagnosis without it changing

## What RSS means here

The memory value shown in the overlay is RSS.

In this project that means:

- resident memory currently in RAM
- grouped across related app processes
- not just one child PID
- not virtual memory

So if the app is multi-process, the RSS number can look bigger than one single process shown in `top`, because the overlay is showing grouped app memory.

## Why the numbers may not match `top` exactly

This is normal.

Reasons:

- the overlay groups related processes for some apps
- `top` may be showing one child process
- CPU changes depend on the sampling window
- multi-process apps can shift work between helper processes

The overlay is meant to be useful and readable while you work, not a perfect clone of `top`.

## Common issues

### `xdotool is required`

Install it first:

Ubuntu / Debian:

```bash
sudo apt install xdotool
```

Arch:

```bash
sudo pacman -S xdotool
```

Fedora:

```bash
sudo dnf install xdotool
```

### Tkinter is missing

Install the Tk package for Python.

Ubuntu / Debian:

```bash
sudo apt install python3-tk
```

Fedora:

```bash
sudo dnf install python3-tkinter
```

Arch usually ships Tk separately as:

```bash
sudo pacman -S tk
```

### The overlay does not detect the active app properly

Check:

- you are running in a Linux desktop session
- on X11, `xdotool` works in your environment
- the app is not blocked by special sandboxing/window rules

On GNOME Wayland (including Ubuntu 25.10 and newer), selecting a process with
`TARGET` is expected. Ubuntu's GNOME session no longer offers the old "Ubuntu
on Xorg" login option.

You can test `xdotool` manually with:

```bash
xdotool getwindowfocus getwindowpid
```

### The overlay opens but some alerts seem limited

That can happen with unusual sandboxed or multi-process apps. The grouping logic works well for many apps, but it is still best-effort.

## Project files

- `overlay_design.py` -> main overlay app
- `start_overlay.sh` -> simple launcher
- `process_time.py` -> older standalone helper script / earlier experiment

## Quick start

If you just want the shortest version:

```bash
git clone https://github.com/codeafridi/Debug-Overlay-App.git
cd Debug-overlay
sudo apt install python3 python3-venv python3-tk xdotool
python3 -m venv venv
source venv/bin/activate
./start_overlay.sh
```
will AUX soon
That is all you need for the current version.
