#include "Config.h"

#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>

#include <algorithm>

namespace {

double clampDouble(double value, double low, double high) {
    return std::clamp(value, low, high);
}

int clampInt(int value, int low, int high) {
    return std::clamp(value, low, high);
}

void validate(Config &config) {
    config.health.cpuWarnPercent =
        clampDouble(config.health.cpuWarnPercent, 0.0, 100.0);
    config.health.cpuHighPercent =
        clampDouble(config.health.cpuHighPercent, 0.0, 100.0);
    if (config.health.cpuHighPercent < config.health.cpuWarnPercent) {
        config.health.cpuHighPercent = config.health.cpuWarnPercent;
    }

    config.health.sustainedSamples =
        clampInt(config.health.sustainedSamples, 1, 100);
    config.health.rssWarnKb = std::min<qulonglong>(
        config.health.rssWarnKb, 1024ULL * 1024ULL * 1024ULL * 1024ULL);

    config.docker.healthyMs = clampInt(config.docker.healthyMs, 1000, 3600000);
    config.docker.issueMs = clampInt(config.docker.issueMs, 1000, 3600000);
    config.docker.backoffStartMs =
        clampInt(config.docker.backoffStartMs, 1000, 3600000);
    config.docker.backoffCapMs =
        clampInt(config.docker.backoffCapMs, 1000, 3600000);
    if (config.docker.backoffCapMs < config.docker.backoffStartMs) {
        config.docker.backoffCapMs = config.docker.backoffStartMs;
    }
    config.docker.absentMs = clampInt(config.docker.absentMs, 1000, 3600000);
    config.docker.timeoutMs = clampInt(config.docker.timeoutMs, 100, 600000);
    config.docker.killGraceMs =
        clampInt(config.docker.killGraceMs, 50, 60000);

    config.focusCacheTtlMs = clampInt(config.focusCacheTtlMs, 0, 600000);
    config.sampleIntervalMs = clampInt(config.sampleIntervalMs, 100, 600000);
}

} // namespace

Config defaultConfig() {
    return Config{};
}

Config loadConfig(const QString &path, QString *errorMessage) {
    Config config;

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return config;
    }

    QSettings settings(path, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage =
                QStringLiteral("could not parse config: %1").arg(path);
        }
        return config;
    }

    settings.beginGroup(QStringLiteral("health"));
    {
        bool ok = false;
        const double warn =
            settings.value(QStringLiteral("cpu_warn_percent")).toString().toDouble(&ok);
        if (ok) {
            config.health.cpuWarnPercent = warn;
        }
        ok = false;
        const double high =
            settings.value(QStringLiteral("cpu_high_percent")).toString().toDouble(&ok);
        if (ok) {
            config.health.cpuHighPercent = high;
        }
        const int rss =
            settings.value(QStringLiteral("rss_warn_kb")).toInt();
        if (rss > 0) {
            config.health.rssWarnKb = static_cast<qulonglong>(rss);
        }
        const int sustain =
            settings.value(QStringLiteral("sustained_samples")).toInt();
        if (sustain > 0) {
            config.health.sustainedSamples = sustain;
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("docker"));
    {
        const int healthy = settings.value(QStringLiteral("healthy_ms")).toInt();
        if (healthy > 0) {
            config.docker.healthyMs = healthy;
        }
        const int issue = settings.value(QStringLiteral("issue_ms")).toInt();
        if (issue > 0) {
            config.docker.issueMs = issue;
        }
        const int backoff =
            settings.value(QStringLiteral("backoff_start_ms")).toInt();
        if (backoff > 0) {
            config.docker.backoffStartMs = backoff;
        }
        const int cap = settings.value(QStringLiteral("backoff_cap_ms")).toInt();
        if (cap > 0) {
            config.docker.backoffCapMs = cap;
        }
        const int absent = settings.value(QStringLiteral("absent_ms")).toInt();
        if (absent > 0) {
            config.docker.absentMs = absent;
        }
        const int timeout = settings.value(QStringLiteral("timeout_ms")).toInt();
        if (timeout > 0) {
            config.docker.timeoutMs = timeout;
        }
    }
    settings.endGroup();

    settings.beginGroup(QStringLiteral("overlay"));
    {
        const int ttl =
            settings.value(QStringLiteral("focus_cache_ttl_ms")).toInt();
        if (ttl >= 0 && settings.contains(QStringLiteral("focus_cache_ttl_ms"))) {
            config.focusCacheTtlMs = ttl;
        }
        const int sample =
            settings.value(QStringLiteral("sample_interval_ms")).toInt();
        if (sample > 0) {
            config.sampleIntervalMs = sample;
        }
    }
    settings.endGroup();

    validate(config);
    return config;
}

QString defaultConfigPath() {
    const QString override =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("DEBUG_OVERLAY_CONFIG"));
    if (!override.isEmpty()) {
        return override;
    }

    const QString configHome =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("XDG_CONFIG_HOME"),
            QProcessEnvironment::systemEnvironment().value(
                QStringLiteral("HOME")) + QStringLiteral("/.config"));
    return configHome + QStringLiteral("/debug-overlay/config.ini");
}
