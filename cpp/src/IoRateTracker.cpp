#include "IoRateTracker.h"

IoRateTracker::Rate IoRateTracker::update(int pid, long long readBytes,
                                          long long writeBytes) {
    Rate rate;
    if (readBytes < 0 && writeBytes < 0) {
        return rate;
    }

    if (!m_clock.isValid()) {
        m_clock.start();
    }
    const qint64 now = m_clock.elapsed();

    const auto it = m_last.constFind(pid);
    if (it != m_last.constEnd()) {
        const double seconds = static_cast<double>(now - it->ms) / 1000.0;
        if (seconds > 0.0) {
            if (readBytes >= 0 && it->read >= 0 && readBytes >= it->read) {
                rate.readPerSec =
                    static_cast<double>(readBytes - it->read) / seconds;
            }
            if (writeBytes >= 0 && it->write >= 0 && writeBytes >= it->write) {
                rate.writePerSec =
                    static_cast<double>(writeBytes - it->write) / seconds;
            }
        }
    }

    Entry entry;
    entry.read = readBytes;
    entry.write = writeBytes;
    entry.ms = now;
    m_last.insert(pid, entry);
    return rate;
}

void IoRateTracker::reset() {
    m_last.clear();
    m_clock.invalidate();
}
