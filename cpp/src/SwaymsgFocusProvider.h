#pragma once

#include "FocusedWindowProvider.h"

#include <QByteArray>
#include <QString>

class SwaymsgFocusProvider : public FocusedWindowProvider {
public:
    explicit SwaymsgFocusProvider(QString command = QStringLiteral("swaymsg"));

    QString name() const override;
    int focusedPid() const override;

private:
    QString m_command;
};

int parseSwayFocusedPid(const QByteArray &output);
