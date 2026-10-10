#pragma once

#include "FocusedWindowProvider.h"

#include <QByteArray>
#include <QString>

class HyprctlFocusProvider : public FocusedWindowProvider {
public:
    explicit HyprctlFocusProvider(QString command = QStringLiteral("hyprctl"));

    QString name() const override;
    int focusedPid() const override;

private:
    QString m_command;
};

int parseHyprctlPid(const QByteArray &output);
