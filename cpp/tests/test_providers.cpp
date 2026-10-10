#include "FocusProviderFactory.h"
#include "FocusedWindowProvider.h"
#include "HyprctlFocusProvider.h"
#include "LinuxProcess.h"
#include "NullFocusProvider.h"
#include "XdotoolFocusProvider.h"

#include <QCoreApplication>

#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    check(selectProviderKind(QStringLiteral("wayland"),
                             QStringLiteral("ubuntu:GNOME"), true)
              == FocusProviderKind::Gnome,
          "wayland+GNOME -> Gnome");
    check(selectProviderKind(QStringLiteral("wayland"),
                             QStringLiteral("KDE"), true)
              == FocusProviderKind::Null,
          "wayland+KDE -> Null (not implemented yet)");
    check(selectProviderKind(QStringLiteral("wayland"),
                             QStringLiteral("Hyprland"), true)
              == FocusProviderKind::Hyprland,
          "wayland+Hyprland -> Hyprland");
    check(selectProviderKind(QStringLiteral("wayland"), QString(), false, true)
              == FocusProviderKind::Hyprland,
          "wayland + HYPRLAND_INSTANCE_SIGNATURE -> Hyprland");
    check(selectProviderKind(QStringLiteral("wayland"), QString(), false)
              == FocusProviderKind::Null,
          "wayland+unknown -> Null");
    check(selectProviderKind(QStringLiteral("x11"),
                             QStringLiteral("ubuntu:GNOME"), true)
              == FocusProviderKind::X11,
          "x11 -> X11");
    check(selectProviderKind(QString(), QString(), true) == FocusProviderKind::X11,
          "empty session + display -> X11");
    check(selectProviderKind(QString(), QString(), false)
              == FocusProviderKind::Null,
          "empty session + no display -> Null");

    check(focusProviderKindName(FocusProviderKind::Gnome)
              == QStringLiteral("gnome-pidfile"),
          "Gnome kind name");
    check(focusProviderKindName(FocusProviderKind::X11)
              == QStringLiteral("xdotool"),
          "X11 kind name");
    check(focusProviderKindName(FocusProviderKind::Hyprland)
              == QStringLiteral("hyprctl"),
          "Hyprland kind name");

    check(parseHyprctlPid(
              QByteArrayLiteral("{\"address\":\"0x1\",\"pid\":4242,\"class\":\"x\"}"))
              == 4242,
          "parseHyprctlPid extracts pid");
    check(parseHyprctlPid(QByteArrayLiteral("{}")) == -1,
          "parseHyprctlPid empty object -> -1");
    check(parseHyprctlPid(QByteArrayLiteral("No active window")) == -1,
          "parseHyprctlPid non-json -> -1");
    check(parseHyprctlPid(QByteArrayLiteral("{\"address\":\"0x1\"}")) == -1,
          "parseHyprctlPid missing pid -> -1");

    HyprctlFocusProvider missingHyprctl(QStringLiteral("/nonexistent/hyprctl"));
    check(missingHyprctl.name() == QStringLiteral("hyprctl"),
          "hyprctl provider name");
    check(missingHyprctl.focusedPid() == -1,
          "missing hyprctl command -> -1");

    NullFocusProvider nullProvider;
    check(nullProvider.name() == QStringLiteral("null"), "null provider name");
    check(nullProvider.focusedPid() == -1, "null provider returns -1");

    XdotoolFocusProvider missing(QStringLiteral("/nonexistent/xdotool"));
    check(missing.name() == QStringLiteral("xdotool"), "xdotool provider name");
    check(missing.focusedPid() == -1, "missing xdotool command -> -1");

    XdotoolFocusProvider real;
    const int pid = real.focusedPid();
    check(pid == -1 || linuxproc::exists(pid),
          "xdotool provider returns -1 or a live pid");

    std::printf("xdotool focusedPid() now = %d\n", pid);

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
