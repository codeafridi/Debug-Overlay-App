#include "CachingFocusProvider.h"

#include <QDateTime>

CachingFocusProvider::CachingFocusProvider(
    std::unique_ptr<FocusedWindowProvider> inner,
    qint64 ttlMs,
    std::function<qint64()> clock)
    : m_inner(std::move(inner))
    , m_ttlMs(ttlMs > 0 ? ttlMs : 1500)
    , m_clock(std::move(clock)) {
}

qint64 CachingFocusProvider::nowMs() const {
    return m_clock ? m_clock() : QDateTime::currentMSecsSinceEpoch();
}

QString CachingFocusProvider::name() const {
    return m_inner ? m_inner->name() : QStringLiteral("cache");
}

int CachingFocusProvider::focusedPid() const {
    if (m_inner == nullptr) {
        return -1;
    }

    const int pid = m_inner->focusedPid();
    if (pid > 0) {
        m_cachedPid = pid;
        m_cachedAt = nowMs();
        return pid;
    }

    if (m_cachedPid > 0 && (nowMs() - m_cachedAt) <= m_ttlMs) {
        return m_cachedPid;
    }

    m_cachedPid = -1;
    return -1;
}
