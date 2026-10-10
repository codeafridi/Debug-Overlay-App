#pragma once

#include <QString>

#include <memory>

class FocusedWindowProvider;

enum class FocusProviderKind {
    Gnome,
    Hyprland,
    X11,
    Null,
};

FocusProviderKind selectProviderKind(const QString &sessionType,
                                     const QString &desktop,
                                     bool hasDisplay,
                                     bool hasHyprlandSignature = false);

std::unique_ptr<FocusedWindowProvider> createFocusProvider();

QString focusProviderKindName(FocusProviderKind kind);
