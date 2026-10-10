#pragma once

#include "ProcessIdentity.h"

#include <QList>

struct CpuSample {
    bool available = false;
    bool warming = false;
    double percent = 0.0;
};

class CpuSampler {
public:
    CpuSample sample(const QList<int> &pids, const ProcessIdentity &key);

private:
    void reset();

    ProcessIdentity m_key;
    QList<int> m_lastPids;
    unsigned long long m_lastProcTicks = 0;
    unsigned long long m_lastTotalTicks = 0;
    bool m_hasBaseline = false;
};
