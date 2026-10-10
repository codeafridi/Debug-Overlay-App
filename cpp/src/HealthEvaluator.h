#pragma once

#include <QString>

class HealthEvaluator {
public:
    enum class Level { Ok, Warn, HighCpu };

    static constexpr double kCpuWarnPercent = 70.0;
    static constexpr double kCpuHighPercent = 90.0;
    static constexpr unsigned long long kRssWarnKb = 1024ULL * 1024ULL;
    static constexpr int kSustainedSamples = 3;

    Level update(bool cpuValid,
                 double cpuPercent,
                 bool rssValid,
                 unsigned long long rssKb);

    void reset();

    static QString text(Level level);

private:
    int m_cpuWarnStreak = 0;
    int m_cpuHighStreak = 0;
    int m_rssWarnStreak = 0;
};
