#pragma once

#include "ProcessIdentity.h"

struct CpuSample {
    bool available = false;
    bool warming = false;
    double percent = 0.0;
};

class CpuSampler {
public:
    CpuSample sample(const ProcessIdentity &identity);

private:
    void reset();

    ProcessIdentity m_identity;
    unsigned long long m_lastProcTicks = 0;
    unsigned long long m_lastTotalTicks = 0;
    bool m_hasBaseline = false;
};
