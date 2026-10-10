#pragma once

#include "FocusedWindowProvider.h"

class NullFocusProvider : public FocusedWindowProvider {
public:
    QString name() const override;
    int focusedPid() const override;
};
