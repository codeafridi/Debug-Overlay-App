# Syspeek on Flatpak

`io.github.codeafridi.Syspeek.yaml` builds Syspeek against the KDE runtime and
packages it as a Flatpak. It is a starting point, not a finished Flathub-grade
manifest. Read the sandbox notes below before shipping it.

## Build

```bash
flatpak install flathub org.kde.Sdk//6.9 org.kde.Platform//6.9
flatpak-builder --user --install --force-clean build-dir \
  cpp/packaging/flatpak/io.github.codeafridi.Syspeek.yaml
flatpak run io.github.codeafridi.Syspeek
```

Adjust `runtime-version` to match the `org.kde.Platform` branch that is current
when you build (6.9 at the time of writing; Flathub rolls these forward).

## What works as-is

- The HUD window itself: Qt 6 comes from the runtime, so no bundling needed.
- The CPU/RSS sampling of the focused app, which reads `/proc` (visible in the
  sandbox).
- The KDE focus bridge, because the manifest owns `org.syspeek.Focus`.

## Host command execution

Syspeek asks the host for state by running host binaries and reading host
files. The app now routes external commands through `flatpak-spawn --host`
automatically whenever `FLATPAK_ID` is set (see `cpp/src/HostCommand.cpp`), so
`docker`, `kubectl`, and the compositor focus tools are executed on the host
instead of inside the sandbox. Outside Flatpak the same code path runs the
binary directly, so normal installs are unchanged.

| Feature | Mechanism | Status under Flatpak |
| --- | --- | --- |
| Docker status | `docker ps` via `flatpak-spawn --host` | works if docker is on the host |
| Kubernetes status | `kubectl get ...` via `flatpak-spawn --host` | works if kubectl + `~/.kube` are available |
| Sway / niri / Hyprland / X11 focus | `flatpak-spawn --host swaymsg/niri/hyprctl/xdotool` | needs the matching host socket exposed |
| GNOME focus bridge | reads `$XDG_RUNTIME_DIR/syspeek-active-pid` | works once the host file exists and is bound in |
| KDE focus bridge | owns `org.syspeek.Focus` | works (`--own-name=org.syspeek.Focus`) |
| Docker socket | `/var/run/docker.sock` | add `--filesystem=/run/docker.sock` if the CLI needs it directly |

## Still to verify

- **PID namespace.** Syspeek samples `/proc/<pid>` for the focused app. Confirm
  the sandbox can see host PIDs on your Flatpak version; if not, the metrics
  columns will show `--` even though focus detection works.
- **Focus sockets.** The Wayland/X11 focus tools need the compositor socket
  reachable from the sandbox. Add the specific socket via `finish-args` for the
  desktops you claim to support.
- Test the GNOME and KDE bridges from a real sandboxed run before publishing.

## Why Flatpak is a bigger job than it looks

The tool is PID-centric and deliberately shells out to whatever the distro
provides. Flatpak's whole value is isolation from exactly that. The
`flatpak-spawn` indirection closes most of the gap, but interfaces, portals, and
the runtime all behave differently from a normal install, so test every desktop
you claim to support.

For a low-effort, fully working distribution channel today, prefer the AppImage
(`../build_appimage.sh`) or the portable tarball: they run on the host and keep
`docker`, `kubectl`, and the focus tools working exactly as the source build
does.

