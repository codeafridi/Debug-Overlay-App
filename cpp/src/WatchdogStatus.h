#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

// Anomaly models (mirroring the forecasting/robust-stat functions used by
// metrics engines such as Prometheus: avg/stddev, quantile, predict_linear).
enum class AnomalyModel { ZScore, Mad, Quantile, PredictLinear };

struct AnomalyResult {
    bool anomalous = false;
    double score = 0.0;
    double value = 0.0;
    double expected = 0.0;
    double lower = 0.0;
    double upper = 0.0;
    QString model;
};

struct WatchdogMetric {
    QString name;
    AnomalyResult result;
};

QString anomalyModelName(AnomalyModel model);
AnomalyResult detectAnomaly(const QList<double> &series, AnomalyModel model,
                            double threshold);
QString watchdogText(const QList<WatchdogMetric> &metrics);
