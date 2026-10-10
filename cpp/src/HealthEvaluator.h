#pragma once

#include "Config.h"

#include <QString>

class HealthEvaluator {
public:
    enum class Level { Ok, Warn, HighCpu };

    HealthEvaluator();
    explicit HealthEvaluator(const HealthConfig &config);

    Level update(bool cpuValid,
                 double cpuPercent,
                 bool rssValid,
                 unsigned long long rssKb);

    void reset();

    static QString text(Level level);

private:
    HealthConfig m_config;
    int m_cpuWarnStreak = 0;
    int m_cpuHighStreak = 0;
    int m_rssWarnStreak = 0;
};
