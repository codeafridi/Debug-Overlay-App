#pragma once

#include <QElapsedTimer>
#include <QHash>

// Per-pid /proc/<pid>/io counter deltas turned into bytes/second.
class IoRateTracker {
public:
    struct Rate {
        double readPerSec = 0.0;
        double writePerSec = 0.0;
    };

    Rate update(int pid, long long readBytes, long long writeBytes);
    void reset();

private:
    struct Entry {
        long long read = -1;
        long long write = -1;
        qint64 ms = 0;
    };

    QHash<int, Entry> m_last;
    QElapsedTimer m_clock;
};
