#include "KdeFocusProvider.h"

#include "KdeFocusState.h"

#include <QDateTime>

KdeFocusProvider::KdeFocusProvider(qint64 ttlMs, std::function<qint64()> clock)
    : m_ttlMs(ttlMs > 0 ? ttlMs : 5000)
    , m_clock(std::move(clock)) {
}

qint64 KdeFocusProvider::nowMs() const {
    return m_clock ? m_clock() : QDateTime::currentMSecsSinceEpoch();
}

QString KdeFocusProvider::name() const {
    return QStringLiteral("kde-dbus");
}

int KdeFocusProvider::focusedPid() const {
    const KdeFocusState &state = kdeFocusState();
    const int pid = state.pid.load();

    if (pid <= 0) {
        return -1;
    }

    if ((nowMs() - state.stampMs.load()) > m_ttlMs) {
        return -1;
    }

    return pid;
}
