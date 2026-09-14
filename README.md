# Debug Overlay

Debug Overlay is a small Linux window that sits on your screen and watches the
app you are using. It shows the app name, PID, CPU use, memory use, and simple
warnings when something looks wrong.

It is made for people who want a quick answer while working: "is this app using
too much CPU or memory?" You do not need to keep `top` open all the time.

<img width="1154" height="774" alt="Debug Overlay screenshot" src="https://github.com/user-attachments/assets/1a6cc9d0-1164-48ba-831e-abed1bbc1701" />

## What it shows

- the app you are currently using
- the app PID
- CPU usage
- RAM usage (RSS)
- Docker container status
- unhealthy Docker containers
- warnings for high CPU, memory growth, disk pressure, network activity, and
  recent system errors

## Docker monitoring

If Docker is installed and accessible to your user, Debug Overlay automatically
checks Docker containers in the background.

The compact overlay shows a short Docker status such as:

```text
docker: 2 running | healthy

Apps such as Cursor, VS Code, Chrome, and Firefox use helper processes. The
overlay groups related child processes, so its CPU and RAM numbers are more
useful than looking at only one helper process.

## Will it work on my Linux system?

The overlay needs a graphical Linux desktop, Python 3, Tkinter, and the Linux
`/proc` filesystem. It works on Ubuntu, Debian, Linux Mint, Fedora, Red Hat,
Arch, and similar distributions.

How it finds the app you are currently using depends on your desktop session:

| Your session | How app detection works |
| --- | --- |
| X11 / Xorg | Automatic, using `xdotool` |
| Ubuntu / GNOME Wayland | Automatic, using the included GNOME integration |
| Hyprland Wayland | Automatic, using `hyprctl` |
| Sway Wayland | Automatic, using `swaymsg` |
| Other Wayland desktops | Need their own compositor integration |

This is not about the Linux distribution alone. Wayland desktop environments do
not share one standard way to reveal the focused app to another program.

## Ubuntu 26.04 / GNOME Wayland setup

This is the setup most Ubuntu users need. Run these commands from the project
folder:

```bash
sudo apt update
sudo apt install python3 python3-tk
bash ./install_gnome_extension.sh
```

Now log out of Ubuntu and log back in once. This is **not** a device restart.
GNOME only loads a newly installed extension when a new desktop session starts.
After that one-time step, the overlay tracks your active app live every time it
runs.

Then start it:

```bash
bash ./start_overlay.sh
```

Do not use `sudo` with either project script.

## X11 / Xorg setup

If your desktop session is X11/Xorg, install `xdotool` too.

Ubuntu, Debian, or Mint:

```bash
sudo apt update
sudo apt install python3 python3-tk xdotool
```

Fedora or Red Hat:

```bash
sudo dnf install python3 python3-tkinter xdotool
```

Arch:

```bash
sudo pacman -S python tk xdotool
```

Start the overlay:

```bash
bash ./start_overlay.sh
```

## Start it automatically

You do not need a server.

If you want the overlay to keep running even after you close Terminal, Cursor,
or VS Code, install the included user service:

```bash
bash ./install_autostart.sh
```

After this, the overlay starts automatically whenever you log in. If it closes
or crashes, systemd starts it again after a few seconds. You do **not** need to
start it manually every day.

It cannot show before you log in, because it is a graphical window and your
desktop does not exist until you log in.

Useful commands:

```bash
# See whether it is running
systemctl --user status debug-overlay.service

# Restart it after changing overlay_design.py
systemctl --user restart debug-overlay.service

# Stop it and stop automatic startup
systemctl --user disable --now debug-overlay.service
```

## Using the overlay

- Drag it anywhere on the screen.
- `HIDE` keeps warnings small: one line with app name, PID, CPU, RSS, and
  `WARN` or `CRITICAL`.
- `SHOW` brings back the full diagnosis panel when a warning appears.
- `MORE` opens the full diagnosis panel whenever you want it.
- `LESS` closes that panel.
- `FRZ` freezes the current numbers so you can read them.

The normal compact overlay is intentionally small. Use `HIDE` if you want
warnings to stay small too, then press `SHOW` or `MORE` when you are actually
debugging something.

## What RSS means

RSS means the RAM currently used by the app. For multi-process apps, the
overlay adds RAM used by related helper processes. That is why its memory number
can be larger than the number for one process shown in `top`.

The numbers may not exactly match `top` because CPU changes every second and
the overlay can group several related processes together. That is normal.

## If something does not work

### The overlay says GNOME focus integration is not enabled

Run this once, without `sudo`:

```bash
bash ./install_gnome_extension.sh
```

Then log out and back in. If it still does not work, check that the bridge is
reporting a PID:

```bash
cat "$XDG_RUNTIME_DIR/debug-overlay-active-pid"
```

Switch to another app and run the command again. The number should change.

### `xdotool` is missing

This matters only for X11/Xorg. Install it with your distribution package
manager; the X11 setup commands above include it.

### Tkinter is missing

Ubuntu/Debian/Mint:

```bash
sudo apt install python3-tk
```

Fedora/RHEL:

```bash
sudo dnf install python3-tkinter
```

Arch:

```bash
sudo pacman -S tk
```

## Project files

- `overlay_design.py` — the main overlay
- `start_overlay.sh` — starts the overlay
- `install_autostart.sh` — makes it start automatically at login
- `install_gnome_extension.sh` — enables automatic app detection for GNOME
  Wayland
- `process_time.py` — older terminal-only experiment
