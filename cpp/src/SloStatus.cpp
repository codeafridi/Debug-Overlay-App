#include "SloStatus.h"

#include <algorithm>

namespace {
constexpr qint64 kMsPerHour = 3600LL * 1000LL;
}

SloReport evaluateSlo(const SloSpec &spec, const QList<SloSample> &samples) {
    SloReport report;
    report.available = true;
    report.name = spec.name;
    report.objective = spec.objective;
    if (samples.isEmpty()) {
        report.sli = 1.0;
        return report;
    }

    qint64 latest = 0;
    for (const SloSample &sample : samples) {
        latest = std::max(latest, sample.atMs);
    }
    const qint64 cutoff =
        latest - static_cast<qint64>(spec.windowHours * kMsPerHour);
    const double allowed = std::max(1e-9, 1.0 - spec.objective);

    for (const SloSample &sample : samples) {
        if (sample.atMs < cutoff) {
            continue;
        }
        ++report.total;
        if (sample.good) {
            ++report.good;
        } else {
            ++report.bad;
        }
    }

    report.sli = report.total > 0
                     ? static_cast<double>(report.good) / report.total
                     : 1.0;
    const double badFraction =
        report.total > 0 ? static_cast<double>(report.bad) / report.total : 0.0;
    report.budgetRemaining = 1.0 - badFraction / allowed;
    report.breached = report.budgetRemaining < 0.0;

    struct WindowDef {
        const char *label;
        double hours;
    };
    static const WindowDef defs[] = {{"1h", 1.0}, {"6h", 6.0}, {"3d", 72.0}};
    for (const WindowDef &def : defs) {
        const qint64 winCutoff =
            latest - static_cast<qint64>(def.hours * kMsPerHour);
        quint64 total = 0;
        quint64 bad = 0;
        for (const SloSample &sample : samples) {
            if (sample.atMs < winCutoff) {
                continue;
            }
            ++total;
            if (!sample.good) {
                ++bad;
            }
        }
        SloWindow window;
        window.label = QString::fromLatin1(def.label);
        window.hours = def.hours;
        window.burnRate = total > 0
                              ? (static_cast<double>(bad) / total) / allowed
                              : 0.0;
        report.windows.append(window);
    }
    return report;
}

SloTracker::SloTracker(double keepHours)
    : m_keepHours(keepHours) {
}

void SloTracker::record(bool good, qint64 atMs) {
    SloSample sample;
    sample.good = good;
    sample.atMs = atMs;
    m_samples.append(sample);
    prune(atMs);
}

void SloTracker::reset() {
    m_samples.clear();
}

void SloTracker::prune(qint64 nowMs) {
    const qint64 cutoff =
        nowMs - static_cast<qint64>(m_keepHours * kMsPerHour);
    while (!m_samples.isEmpty() && m_samples.first().atMs < cutoff) {
        m_samples.removeFirst();
    }
    // Hard cap to bound memory even at high sample rates.
    while (m_samples.size() > 200000) {
        m_samples.removeFirst();
    }
}

SloReport SloTracker::report(const SloSpec &spec) const {
    return evaluateSlo(spec, m_samples);
}

QString sloSummaryText(const SloReport &report) {
    if (!report.available || report.total == 0) {
        return QStringLiteral("slo: collecting samples");
    }
    return QStringLiteral("objective %1%  SLI %2%  budget %3%")
        .arg(QString::number(report.objective * 100.0, 'f', 3),
             QString::number(report.sli * 100.0, 'f', 3),
             QString::number(report.budgetRemaining * 100.0, 'f', 1));
}

QString sloDetailText(const SloReport &report) {
    if (!report.available || report.total == 0) {
        return QStringLiteral("collecting availability samples");
    }
    QString text = QStringLiteral("%1\n%2 good / %3 bad of %4 samples\n"
                                  "status: %5\n")
                       .arg(report.name)
                       .arg(report.good)
                       .arg(report.bad)
                       .arg(report.total)
                       .arg(report.breached ? QStringLiteral("BUDGET EXHAUSTED")
                                            : QStringLiteral("within budget"));
    for (const SloWindow &window : report.windows) {
        text += QStringLiteral("burn %1: %2x\n")
                    .arg(window.label.leftJustified(3, ' '),
                         QString::number(window.burnRate, 'f', 2));
    }
    return text.trimmed();
}
