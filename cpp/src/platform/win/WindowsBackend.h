#pragma once

#include "../SystemBackend.h"

class WindowsBackend : public SystemBackend {
public:
    QString platformName() const override;
    PlatformCapabilities capabilities() const override;
};
