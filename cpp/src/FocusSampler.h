#pragma once

#include "ProcessIdentity.h"

#include <QString>

class FocusedWindowProvider;

struct FocusSample {
    bool available = false;
    ProcessIdentity identity;
    QString appName = QStringLiteral("--");
    QString providerName;
};

class FocusSampler {
public:
    explicit FocusSampler(const FocusedWindowProvider *provider);

    FocusSample sample() const;

private:
    const FocusedWindowProvider *m_provider;
    mutable ProcessIdentity m_lastIdentity;
    mutable QString m_lastName;
};
