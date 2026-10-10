#include "HealthEvaluator.h"

#include <cstdio>

int main() {
    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    constexpr unsigned long long lowRss = 100ULL * 1024;
    constexpr unsigned long long highRss = HealthConfig{}.rssWarnKb + 1;

    {
        HealthEvaluator h;
        check(h.update(true, 10.0, true, lowRss) == HealthEvaluator::Level::Ok,
              "low cpu/rss -> OK");
    }

    {
        HealthEvaluator h;
        HealthEvaluator::Level level = HealthEvaluator::Level::Ok;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            level = h.update(true, 80.0, true, lowRss);
        }
        check(level == HealthEvaluator::Level::Warn,
              "sustained cpu>=warn -> WARN");
    }

    {
        HealthEvaluator h;
        HealthEvaluator::Level level = HealthEvaluator::Level::Ok;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            level = h.update(true, 95.0, true, lowRss);
        }
        check(level == HealthEvaluator::Level::HighCpu,
              "sustained cpu>=high -> HIGH CPU");
    }

    {
        HealthEvaluator h;
        HealthEvaluator::Level level = HealthEvaluator::Level::Ok;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            level = h.update(true, 5.0, true, highRss);
        }
        check(level == HealthEvaluator::Level::Warn,
              "sustained high rss -> WARN");
    }

    {
        HealthEvaluator h;
        h.update(true, 95.0, true, lowRss);
        h.update(true, 95.0, true, lowRss);
        const HealthEvaluator::Level afterTwo = h.update(true, 5.0, true, lowRss);
        check(afterTwo == HealthEvaluator::Level::Ok,
              "single spike then normal stays OK (not sustained)");

        HealthEvaluator::Level level = HealthEvaluator::Level::Ok;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            level = h.update(true, 95.0, true, lowRss);
        }
        check(level == HealthEvaluator::Level::HighCpu,
              "streak re-accumulates after reset");
    }

    {
        HealthEvaluator h;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            h.update(true, 95.0, true, lowRss);
        }
        h.reset();
        check(h.update(true, 95.0, true, lowRss) == HealthEvaluator::Level::Ok,
              "reset clears streaks");
    }

    {
        HealthEvaluator h;
        for (int i = 0; i < HealthConfig{}.sustainedSamples; ++i) {
            h.update(false, 0.0, false, 0);
        }
        check(h.update(false, 0.0, true, lowRss) == HealthEvaluator::Level::Ok,
              "invalid cpu/rss -> OK");
    }

    check(HealthEvaluator::text(HealthEvaluator::Level::Ok) == QStringLiteral("OK"),
          "OK text");
    check(HealthEvaluator::text(HealthEvaluator::Level::Warn) == QStringLiteral("WARN"),
          "WARN text");
    check(HealthEvaluator::text(HealthEvaluator::Level::HighCpu)
              == QStringLiteral("HIGH CPU"),
          "HIGH CPU text");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
