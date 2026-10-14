#include "CostModel.h"

#include <QHash>

#include <algorithm>

namespace {
constexpr double kHoursPerMonth = 730.0;
constexpr double kBytesPerGb = 1024.0 * 1024.0 * 1024.0;
} // namespace

CostSummary computeClusterCost(const QList<KubePod> &pods,
                               double cpuPricePerCoreHour,
                               double memPricePerGbHour) {
    CostSummary summary;
    QHash<QString, double> byNamespace;

    for (const KubePod &pod : pods) {
        const double cpuHour = pod.cpuRequestCores * cpuPricePerCoreHour;
        const double memHour =
            (pod.memRequestBytes / kBytesPerGb) * memPricePerGbHour;
        const double monthly = (cpuHour + memHour) * kHoursPerMonth;

        byNamespace[pod.ns] += monthly;
        summary.totalMonthly += monthly;
        ++summary.podsPriced;
        const int missing =
            pod.containersWithoutCpuRequest + pod.containersWithoutMemRequest;
        summary.missingRequestContainers += missing;

        WorkloadCost workload;
        workload.ns = pod.ns;
        workload.pod = pod.name;
        workload.monthly = monthly;
        workload.missingRequests = missing;
        summary.topWorkloads.append(workload);
    }

    for (auto it = byNamespace.constBegin(); it != byNamespace.constEnd(); ++it) {
        summary.byNamespace.append(qMakePair(it.key(), it.value()));
    }
    std::sort(summary.byNamespace.begin(), summary.byNamespace.end(),
              [](const QPair<QString, double> &a,
                 const QPair<QString, double> &b) {
                  return a.second > b.second;
              });

    std::sort(summary.topWorkloads.begin(), summary.topWorkloads.end(),
              [](const WorkloadCost &a, const WorkloadCost &b) {
                  return a.monthly > b.monthly;
              });
    if (summary.topWorkloads.size() > 20) {
        summary.topWorkloads = summary.topWorkloads.mid(0, 20);
    }

    return summary;
}
