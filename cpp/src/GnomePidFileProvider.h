#pragma once

#include "FocusedWindowProvider.h"

#include <QString>

class GnomePidFileProvider : public FocusedWindowProvider {
public:
    explicit GnomePidFileProvider(QString path = defaultPath());

    QString name() const override;
    int focusedPid() const override;

    static QString defaultPath();

private:
    QString m_path;
};
