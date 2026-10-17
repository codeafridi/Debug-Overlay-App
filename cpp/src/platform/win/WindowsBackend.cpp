#include "WindowsBackend.h"

QString WindowsBackend::platformName() const {
    return QStringLiteral("windows");
}

PlatformCapabilities WindowsBackend::capabilities() const {
    return PlatformCapabilities{};
}
