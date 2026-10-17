#pragma once

#include "../SystemBackend.h"

class MacBackend : public SystemBackend {
public:
    QString platformName() const override;
    PlatformCapabilities capabilities() const override;
};
