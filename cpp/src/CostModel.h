#pragma once

#include "KubeStatus.h"

#include <QList>
#include <QPair>
#include <QString>

struct WorkloadCost {
    QString ns;
    QString pod;
    double monthly = 0.0;
    int missingRequests = 0;
};

struct CostSummary {
    QList<QPair<QString, double>> byNamespace; // monthly, desc
    QList<WorkloadCost> topWorkloads;          // monthly, desc, capped
    double totalMonthly = 0.0;
    int missingRequestContainers = 0;
    int podsPriced = 0;
};

// Requests-based monthly estimate (OpenCost-style allocation).
CostSummary computeClusterCost(const QList<KubePod> &pods,
                               double cpuPricePerCoreHour,
                               double memPricePerGbHour);
