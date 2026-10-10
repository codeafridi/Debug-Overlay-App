#include "CpuSampler.h"

#include "LinuxProcess.h"

#include <algorithm>
#include <thread>

namespace {

QList<int> normalized(const QList<int> &pids) {
    QList<int> copy = pids;
    std::sort(copy.begin(), copy.end());
    return copy;
}

} // namespace

CpuSample CpuSampler::sample(const QList<int> &pids, const ProcessIdentity &key) {
    CpuSample result;

    if (!key.isValid() || pids.isEmpty()) {
        reset();
        return result;
    }

    unsigned long long total = 0;
    if (!linuxproc::totalCpuTicks(total)) {
        reset();
        return result;
    }

    unsigned long long sum = 0;
    bool any = false;
    for (int pid : pids) {
        unsigned long long ticks = 0;
        if (linuxproc::cpuTicks(pid, ticks)) {
            sum += ticks;
            any = true;
        }
    }

    if (!any) {
        reset();
        return result;
    }

    const QList<int> current = normalized(pids);
    if (!m_hasBaseline || key != m_key || current != m_lastPids) {
        m_key = key;
        m_lastPids = current;
        m_lastProcTicks = sum;
        m_lastTotalTicks = total;
        m_hasBaseline = true;
        result.available = true;
        result.warming = true;
        return result;
    }

    const long long deltaProc =
        static_cast<long long>(sum) - static_cast<long long>(m_lastProcTicks);
    const long long deltaTotal =
        static_cast<long long>(total) - static_cast<long long>(m_lastTotalTicks);

    m_lastProcTicks = sum;
    m_lastTotalTicks = total;

    if (deltaProc < 0 || deltaTotal <= 0) {
        reset();
        result.available = true;
        result.warming = true;
        return result;
    }

    const unsigned int cores = std::max(1u, std::thread::hardware_concurrency());
    const double percent = static_cast<double>(deltaProc)
                           / static_cast<double>(deltaTotal) * 100.0
                           * static_cast<double>(cores);

    result.available = true;
    result.warming = false;
    result.percent = std::clamp(percent, 0.0, 999.0 * cores);
    return result;
}

void CpuSampler::reset() {
    m_key = ProcessIdentity{};
    m_lastPids.clear();
    m_lastProcTicks = 0;
    m_lastTotalTicks = 0;
    m_hasBaseline = false;
}
