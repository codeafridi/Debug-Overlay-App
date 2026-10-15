#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

struct SloSpec {
    QString name = QStringLiteral("focused-app availability");
    double objective = 0.999;   // target success fraction
    double windowHours = 720.0; // 30 days
};

struct SloSample {
    qint64 atMs = 0;
    bool good = true;
};

struct SloWindow {
    QString label;      // e.g. "1h"
    double hours = 0.0;
    double burnRate = 0.0;
};

struct SloReport {
    bool available = false;
    QString name;
    double objective = 0.0;
    double sli = 1.0;
    quint64 total = 0;
    quint64 good = 0;
    quint64 bad = 0;
    double budgetRemaining = 1.0; // fraction of error budget left
    bool breached = false;
    QList<SloWindow> windows;
};

SloReport evaluateSlo(const SloSpec &spec, const QList<SloSample> &samples);

class SloTracker {
public:
    explicit SloTracker(double keepHours = 720.0);
    void record(bool good, qint64 atMs);
    void reset();
    SloReport report(const SloSpec &spec) const;

private:
    void prune(qint64 nowMs);
    QList<SloSample> m_samples;
    double m_keepHours;
};

QString sloSummaryText(const SloReport &report);
QString sloDetailText(const SloReport &report);
