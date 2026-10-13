#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>

// Per-interface /proc/net/dev counter deltas turned into bytes/second.
class NetRateTracker {
public:
    struct Rate {
        double rxPerSec = 0.0;
        double txPerSec = 0.0;
    };

    Rate update(const QString &name, unsigned long long rxBytes,
                unsigned long long txBytes);
    void reset();

private:
    struct Entry {
        unsigned long long rx = 0;
        unsigned long long tx = 0;
        qint64 ms = 0;
    };

    QHash<QString, Entry> m_last;
    QElapsedTimer m_clock;
};
