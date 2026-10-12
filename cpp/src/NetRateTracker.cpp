#include "NetRateTracker.h"

NetRateTracker::Rate NetRateTracker::update(const QString &name,
                                            unsigned long long rxBytes,
                                            unsigned long long txBytes) {
    Rate rate;

    if (!m_clock.isValid()) {
        m_clock.start();
    }
    const qint64 now = m_clock.elapsed();

    const auto it = m_last.constFind(name);
    if (it != m_last.constEnd()) {
        const double seconds = static_cast<double>(now - it->ms) / 1000.0;
        if (seconds > 0.0) {
            if (rxBytes >= it->rx) {
                rate.rxPerSec =
                    static_cast<double>(rxBytes - it->rx) / seconds;
            }
            if (txBytes >= it->tx) {
                rate.txPerSec =
                    static_cast<double>(txBytes - it->tx) / seconds;
            }
        }
    }

    Entry entry;
    entry.rx = rxBytes;
    entry.tx = txBytes;
    entry.ms = now;
    m_last.insert(name, entry);
    return rate;
}

void NetRateTracker::reset() {
    m_last.clear();
    m_clock.invalidate();
}
