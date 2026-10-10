#pragma once

#include "CpuSampler.h"
#include "Config.h"
#include "DisplaySnapshot.h"
#include "FocusSampler.h"
#include "GroupCache.h"
#include "HealthEvaluator.h"
#include "ProcessIdentity.h"

#include <QObject>
#include <QString>

class FocusedWindowProvider;
class QTimer;

class SamplerWorker : public QObject {
    Q_OBJECT

public:
    explicit SamplerWorker(FocusedWindowProvider *provider,
                           const Config &config = Config{},
                           QObject *parent = nullptr);

public slots:
    void start();
    void stop();
    void sampleNow();

signals:
    void snapshotReady(const DisplaySnapshot &snapshot);

private:
    DisplaySnapshot buildSnapshot(const FocusSample &focus);

    FocusedWindowProvider *m_provider;
    FocusSampler m_sampler;
    CpuSampler m_cpuSampler;
    GroupCache m_groupCache;
    QTimer *m_timer = nullptr;
    int m_intervalMs = 1000;
    quint64 m_sequence = 0;
    QString m_cpuText = QStringLiteral("--");
    QString m_memText = QStringLiteral("--");
    ProcessIdentity m_cpuIdentity;
    ProcessIdentity m_healthIdentity;
    HealthEvaluator m_health;
};
