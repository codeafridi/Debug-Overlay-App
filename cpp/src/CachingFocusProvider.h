#pragma once

#include "FocusedWindowProvider.h"

#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

class CachingFocusProvider : public FocusedWindowProvider {
public:
    explicit CachingFocusProvider(std::unique_ptr<FocusedWindowProvider> inner,
                                  qint64 ttlMs = 1500,
                                  std::function<qint64()> clock = {});

    QString name() const override;
    int focusedPid() const override;

private:
    qint64 nowMs() const;

    std::unique_ptr<FocusedWindowProvider> m_inner;
    qint64 m_ttlMs;
    std::function<qint64()> m_clock;
    mutable int m_cachedPid = -1;
    mutable qint64 m_cachedAt = 0;
};
