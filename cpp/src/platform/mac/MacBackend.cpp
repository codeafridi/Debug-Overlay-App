#include "MacBackend.h"

QString MacBackend::platformName() const {
    return QStringLiteral("macos");
}

PlatformCapabilities MacBackend::capabilities() const {
    return PlatformCapabilities{};
}
