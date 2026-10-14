#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

struct HealthConfig {
    double cpuWarnPercent = 70.0;
    double cpuHighPercent = 90.0;
    qulonglong rssWarnKb = 1024ULL * 1024ULL;
    int sustainedSamples = 3;
};

struct DockerConfig {
    int healthyMs = 20000;
    int issueMs = 8000;
    int backoffStartMs = 15000;
    int backoffCapMs = 240000;
    int absentMs = 300000;
    int timeoutMs = 3000;
    int killGraceMs = 500;
};

struct KubeConfig {
    int healthyMs = 30000;
    int issueMs = 10000;
    int backoffStartMs = 15000;
    int backoffCapMs = 240000;
    int absentMs = 300000;
    int timeoutMs = 5000;
    int killGraceMs = 500;
};

struct SyntheticConfig {
    QStringList targets;
    int intervalMs = 30000;
    int timeoutMs = 5000;
};

struct CostConfig {
    double cpuPerCoreHour = 0.031611;
    double memPerGbHour = 0.004237;
};

struct TraceConfig {
    bool enabled = true;
    int port = 4318;
    int maxSpans = 5000;
    int emitIntervalMs = 1000;
};

struct Config {
    HealthConfig health;
    DockerConfig docker;
    KubeConfig kube;
    SyntheticConfig synthetic;
    CostConfig cost;
    TraceConfig traces;
    int focusCacheTtlMs = 1500;
    int sampleIntervalMs = 1000;
};

Config defaultConfig();

Config loadConfig(const QString &path, QString *errorMessage);

QString defaultConfigPath();
