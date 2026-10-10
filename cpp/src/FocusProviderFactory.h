#pragma once

#include <QString>

#include <memory>

class FocusedWindowProvider;

enum class FocusProviderKind {
    Gnome,
    Kde,
    Hyprland,
    Sway,
    X11,
    Null,
};

struct SessionInfo {
    QString sessionType;
    QString desktop;
    bool hasDisplay = false;
    bool hasHyprlandSignature = false;
    bool hasSwaySocket = false;
};

FocusProviderKind selectProviderKind(const SessionInfo &info);

std::unique_ptr<FocusedWindowProvider> createFocusProvider();

QString focusProviderKindName(FocusProviderKind kind);
