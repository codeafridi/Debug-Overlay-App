#include "FocusProviderFactory.h"

#include "CachingFocusProvider.h"
#include "GnomePidFileProvider.h"
#include "HyprctlFocusProvider.h"
#include "KdeFocusProvider.h"
#include "NullFocusProvider.h"
#include "SwaymsgFocusProvider.h"
#include "XdotoolFocusProvider.h"

#include <QProcessEnvironment>

FocusProviderKind selectProviderKind(const SessionInfo &info) {
    const QString desktop = info.desktop.toLower();

    if (info.sessionType.compare(QStringLiteral("wayland"),
                                 Qt::CaseInsensitive)
        == 0) {
        if (desktop.contains(QStringLiteral("gnome"))) {
            return FocusProviderKind::Gnome;
        }
        if (desktop.contains(QStringLiteral("kde"))
            || desktop.contains(QStringLiteral("plasma"))) {
            return FocusProviderKind::Kde;
        }
        if (desktop.contains(QStringLiteral("hyprland"))
            || info.hasHyprlandSignature) {
            return FocusProviderKind::Hyprland;
        }
        if (desktop.contains(QStringLiteral("sway")) || info.hasSwaySocket) {
            return FocusProviderKind::Sway;
        }
        return FocusProviderKind::Null;
    }

    if (info.hasDisplay) {
        return FocusProviderKind::X11;
    }

    return FocusProviderKind::Null;
}

QString focusProviderKindName(FocusProviderKind kind) {
    switch (kind) {
    case FocusProviderKind::Gnome:
        return QStringLiteral("gnome-pidfile");
    case FocusProviderKind::Kde:
        return QStringLiteral("kde-dbus");
    case FocusProviderKind::Hyprland:
        return QStringLiteral("hyprctl");
    case FocusProviderKind::Sway:
        return QStringLiteral("swaymsg");
    case FocusProviderKind::X11:
        return QStringLiteral("xdotool");
    case FocusProviderKind::Null:
    default:
        return QStringLiteral("null");
    }
}

std::unique_ptr<FocusedWindowProvider> createFocusProvider() {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    SessionInfo info;
    info.sessionType = env.value(QStringLiteral("XDG_SESSION_TYPE"));
    info.desktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    info.hasDisplay = !env.value(QStringLiteral("DISPLAY")).isEmpty();
    info.hasHyprlandSignature =
        !env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE")).isEmpty();
    info.hasSwaySocket = !env.value(QStringLiteral("SWAYSOCK")).isEmpty();

    switch (selectProviderKind(info)) {
    case FocusProviderKind::Gnome:
        return std::make_unique<GnomePidFileProvider>();
    case FocusProviderKind::Kde:
        return std::make_unique<KdeFocusProvider>();
    case FocusProviderKind::Hyprland:
        return std::make_unique<CachingFocusProvider>(
            std::make_unique<HyprctlFocusProvider>());
    case FocusProviderKind::Sway:
        return std::make_unique<CachingFocusProvider>(
            std::make_unique<SwaymsgFocusProvider>());
    case FocusProviderKind::X11:
        return std::make_unique<CachingFocusProvider>(
            std::make_unique<XdotoolFocusProvider>());
    case FocusProviderKind::Null:
    default:
        return std::make_unique<NullFocusProvider>();
    }
}
