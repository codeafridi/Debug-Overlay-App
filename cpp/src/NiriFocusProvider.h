#pragma once

#include "FocusedWindowProvider.h"

#include <QByteArray>
#include <QString>

class NiriFocusProvider : public FocusedWindowProvider {
public:
    explicit NiriFocusProvider(QString command = QStringLiteral("niri"));

    QString name() const override;
    int focusedPid() const override;

private:
    QString m_command;
};

int parseNiriPid(const QByteArray &output);
