#pragma once

#include "DiskStatus.h"

#include <QElapsedTimer>

#include <functional>

struct DiskSource {
    std::function<bool(QByteArray &)> diskstats;
    std::function<bool(QByteArray &)> pressure;

    static DiskSource system();
};

class DiskSampler {
public:
    explicit DiskSampler(DiskSource source = DiskSource::system());

    DiskSnapshot sample();
    void reset();

private:
    DiskSource m_source;
    bool m_hasBaseline = false;
    QList<DiskCounters> m_last;
    QElapsedTimer m_timer;
};
