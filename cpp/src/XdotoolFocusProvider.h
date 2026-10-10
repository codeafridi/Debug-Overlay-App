#pragma once

#include "FocusedWindowProvider.h"

#include <QString>

class XdotoolFocusProvider : public FocusedWindowProvider {
public:
    explicit XdotoolFocusProvider(QString command = QStringLiteral("xdotool"));

    QString name() const override;
    int focusedPid() const override;

private:
    QString m_command;
};
