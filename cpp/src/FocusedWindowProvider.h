#pragma once

#include <QString>

class FocusedWindowProvider {
public:
    virtual ~FocusedWindowProvider() = default;

    virtual QString name() const = 0;
    virtual int focusedPid() const = 0;
};
