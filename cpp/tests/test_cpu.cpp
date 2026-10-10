#include "CpuSampler.h"
#include "LinuxProcess.h"
#include "ProcessIdentity.h"

#include <QCoreApplication>

#include <unistd.h>

#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    unsigned long long total = 0;
    check(linuxproc::totalCpuTicks(total) && total > 0,
          "totalCpuTicks reads a positive value");

    const int self = static_cast<int>(::getpid());
    unsigned long long ticks = 0;
    check(linuxproc::cpuTicks(self, ticks), "cpuTicks(self) read succeeds");
    check(!linuxproc::cpuTicks(1 << 30, ticks), "cpuTicks(bogus) fails");

    CpuSampler sampler;

    const CpuSample invalid = sampler.sample({}, ProcessIdentity{});
    check(!invalid.available, "invalid identity -> unavailable");

    const ProcessIdentity id = linuxproc::identity(self);
    check(id.isValid(), "identity(self) valid");

    const QList<int> pids{self};
    const CpuSample first = sampler.sample(pids, id);
    check(first.available, "first sample available");
    check(first.warming, "first sample warming (baseline)");

    volatile unsigned long long sink = 0;
    for (int i = 0; i < 3000000; ++i) {
        sink += static_cast<unsigned long long>(i);
    }
    ::usleep(60000);
    (void)sink;

    const CpuSample second = sampler.sample(pids, id);
    check(second.available, "second sample available");
    check(!second.warming, "second sample not warming after interval");
    check(second.percent >= 0.0 && second.percent <= 999.0,
          "percent within range");

    const ProcessIdentity reused(self, id.startTime + 1);
    const CpuSample changed = sampler.sample(pids, reused);
    check(changed.available && changed.warming,
          "identity change resets baseline (warming)");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
