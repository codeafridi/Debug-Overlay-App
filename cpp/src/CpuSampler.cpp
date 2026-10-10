#include "CpuSampler.h"

#include "LinuxProcess.h"

#include <algorithm>
#include <thread>

CpuSample CpuSampler::sample(const ProcessIdentity &identity) {
    CpuSample result;

    if (!identity.isValid()) {
        reset();
        return result;
    }

    unsigned long long total = 0;
    unsigned long long proc = 0;
    if (!linuxproc::totalCpuTicks(total) || !linuxproc::cpuTicks(identity.pid, proc)) {
        reset();
        return result;
    }

    if (!m_hasBaseline || identity != m_identity) {
        m_identity = identity;
        m_lastProcTicks = proc;
        m_lastTotalTicks = total;
        m_hasBaseline = true;
        result.available = true;
        result.warming = true;
        return result;
    }

    const long long deltaProc =
        static_cast<long long>(proc) - static_cast<long long>(m_lastProcTicks);
    const long long deltaTotal =
        static_cast<long long>(total) - static_cast<long long>(m_lastTotalTicks);

    m_lastProcTicks = proc;
    m_lastTotalTicks = total;

    if (deltaProc < 0 || deltaTotal <= 0) {
        reset();
        result.available = true;
        result.warming = true;
        return result;
    }

    const unsigned int cores = std::max(1u, std::thread::hardware_concurrency());
    double percent = static_cast<double>(deltaProc)
                     / static_cast<double>(deltaTotal) * 100.0
                     * static_cast<double>(cores);

    result.available = true;
    result.warming = false;
    result.percent = std::clamp(percent, 0.0, 999.0);
    return result;
}

void CpuSampler::reset() {
    m_identity = ProcessIdentity{};
    m_lastProcTicks = 0;
    m_lastTotalTicks = 0;
    m_hasBaseline = false;
}
