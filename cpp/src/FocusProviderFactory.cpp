#include "FocusProviderFactory.h"

#include "GnomePidFileProvider.h"
#include "HyprctlFocusProvider.h"
#include "NullFocusProvider.h"
#include "XdotoolFocusProvider.h"

#include <QProcessEnvironment>

FocusProviderKind selectProviderKind(const QString &sessionType,
                                     const QString &desktop,
                                     bool hasDisplay,
                                     bool hasHyprlandSignature) {
    const QString normalizedDesktop = desktop.toLower();

    if (sessionType.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0) {
        if (normalizedDesktop.contains(QStringLiteral("gnome"))) {
            return FocusProviderKind::Gnome;
        }
        if (normalizedDesktop.contains(QStringLiteral("hyprland"))
            || hasHyprlandSignature) {
            return FocusProviderKind::Hyprland;
        }
        return FocusProviderKind::Null;
    }

    if (hasDisplay) {
        return FocusProviderKind::X11;
    }

    return FocusProviderKind::Null;
}

QString focusProviderKindName(FocusProviderKind kind) {
    switch (kind) {
    case FocusProviderKind::Gnome:
        return QStringLiteral("gnome-pidfile");
    case FocusProviderKind::Hyprland:
        return QStringLiteral("hyprctl");
    case FocusProviderKind::X11:
        return QStringLiteral("xdotool");
    case FocusProviderKind::Null:
    default:
        return QStringLiteral("null");
    }
}

std::unique_ptr<FocusedWindowProvider> createFocusProvider() {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString sessionType = env.value(QStringLiteral("XDG_SESSION_TYPE"));
    const QString desktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    const bool hasDisplay = !env.value(QStringLiteral("DISPLAY")).isEmpty();
    const bool hasHyprlandSignature =
        !env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE")).isEmpty();

    switch (selectProviderKind(sessionType, desktop, hasDisplay,
                               hasHyprlandSignature)) {
    case FocusProviderKind::Gnome:
        return std::make_unique<GnomePidFileProvider>();
    case FocusProviderKind::Hyprland:
        return std::make_unique<HyprctlFocusProvider>();
    case FocusProviderKind::X11:
        return std::make_unique<XdotoolFocusProvider>();
    case FocusProviderKind::Null:
    default:
        return std::make_unique<NullFocusProvider>();
    }
}
