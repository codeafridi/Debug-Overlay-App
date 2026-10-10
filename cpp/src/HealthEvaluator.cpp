#include "HealthEvaluator.h"

HealthEvaluator::HealthEvaluator()
    : m_config() {
}

HealthEvaluator::HealthEvaluator(const HealthConfig &config)
    : m_config(config) {
}

HealthEvaluator::Level HealthEvaluator::update(bool cpuValid,
                                               double cpuPercent,
                                               bool rssValid,
                                               unsigned long long rssKb) {
    if (cpuValid && cpuPercent >= m_config.cpuHighPercent) {
        ++m_cpuHighStreak;
    } else {
        m_cpuHighStreak = 0;
    }

    if (cpuValid && cpuPercent >= m_config.cpuWarnPercent) {
        ++m_cpuWarnStreak;
    } else {
        m_cpuWarnStreak = 0;
    }

    if (rssValid && rssKb >= m_config.rssWarnKb) {
        ++m_rssWarnStreak;
    } else {
        m_rssWarnStreak = 0;
    }

    if (m_cpuHighStreak >= m_config.sustainedSamples) {
        return Level::HighCpu;
    }

    if (m_cpuWarnStreak >= m_config.sustainedSamples
        || m_rssWarnStreak >= m_config.sustainedSamples) {
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
