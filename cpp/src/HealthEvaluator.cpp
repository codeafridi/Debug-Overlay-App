#include "HealthEvaluator.h"

HealthEvaluator::Level HealthEvaluator::update(bool cpuValid,
                                               double cpuPercent,
                                               bool rssValid,
                                               unsigned long long rssKb) {
    if (cpuValid && cpuPercent >= kCpuHighPercent) {
        ++m_cpuHighStreak;
    } else {
        m_cpuHighStreak = 0;
    }

    if (cpuValid && cpuPercent >= kCpuWarnPercent) {
        ++m_cpuWarnStreak;
    } else {
        m_cpuWarnStreak = 0;
    }

    if (rssValid && rssKb >= kRssWarnKb) {
        ++m_rssWarnStreak;
    } else {
        m_rssWarnStreak = 0;
    }

    if (m_cpuHighStreak >= kSustainedSamples) {
        return Level::HighCpu;
    }

    if (m_cpuWarnStreak >= kSustainedSamples
        || m_rssWarnStreak >= kSustainedSamples) {
        return Level::Warn;
    }

    return Level::Ok;
}

void HealthEvaluator::reset() {
    m_cpuWarnStreak = 0;
    m_cpuHighStreak = 0;
    m_rssWarnStreak = 0;
}

QString HealthEvaluator::text(Level level) {
    switch (level) {
    case Level::HighCpu:
        return QStringLiteral("HIGH CPU");
    case Level::Warn:
        return QStringLiteral("WARN");
    case Level::Ok:
    default:
        return QStringLiteral("OK");
    }
}
