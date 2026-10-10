#pragma once

#include "FocusedWindowProvider.h"

#include <QString>
#include <QtGlobal>

#include <functional>

class KdeFocusProvider : public FocusedWindowProvider {
public:
    explicit KdeFocusProvider(qint64 ttlMs = 5000,
                              std::function<qint64()> clock = {});

    QString name() const override;
    int focusedPid() const override;

private:
    qint64 nowMs() const;

    qint64 m_ttlMs;
    std::function<qint64()> m_clock;
};
