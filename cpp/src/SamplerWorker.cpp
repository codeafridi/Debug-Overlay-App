#include "SamplerWorker.h"

#include "FocusedWindowProvider.h"
#include "LinuxProcess.h"
#include "MemoryFormat.h"

#include <QTimer>

SamplerWorker::SamplerWorker(FocusedWindowProvider *provider, QObject *parent)
    : QObject(parent)
    , m_provider(provider)
    , m_sampler(provider) {
}

void SamplerWorker::start() {
    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setInterval(1000);
        connect(m_timer, &QTimer::timeout, this, &SamplerWorker::sampleNow);
    }
    m_timer->start();
    sampleNow();
}

void SamplerWorker::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
}

void SamplerWorker::sampleNow() {
    const FocusSample focus = m_sampler.sample();
    DisplaySnapshot snapshot = buildSnapshot(focus);
    snapshot.sequence = ++m_sequence;
    emit snapshotReady(snapshot);
}

DisplaySnapshot SamplerWorker::buildSnapshot(const FocusSample &focus) {
    DisplaySnapshot snap;

    if (!focus.available) {
        snap.status = QStringLiteral("no-focus");
        m_cpuText = QStringLiteral("--");
        m_memText = QStringLiteral("--");
        m_cpuIdentity = ProcessIdentity{};
        m_healthIdentity = ProcessIdentity{};
        m_health.reset();
        return snap;
    }

    if (focus.identity != m_healthIdentity) {
        m_health.reset();
        m_healthIdentity = focus.identity;
    }

    snap.appName = focus.appName;
    snap.pid = QString::number(focus.identity.pid);

    const ProcessGroup &group = m_groupCache.pidsFor(focus.identity);
    const CpuSample cpu = m_cpuSampler.sample(group.pids, focus.identity);
    const bool cpuValid = cpu.available && !cpu.warming;

    if (!cpu.available) {
        m_cpuText = QStringLiteral("--");
    } else if (cpu.warming) {
        m_cpuText = QStringLiteral("Warming up");
    } else {
        m_cpuText = QStringLiteral("%1%").arg(qRound(cpu.percent));
    }

    unsigned long long rss = 0;
    const bool rssValid = linuxproc::sumRssKb(group.pids, rss);
    m_memText = rssValid ? formatMemoryKb(rss) : QStringLiteral("--");

    m_cpuIdentity = focus.identity;

    snap.cpu = m_cpuText;
    snap.memory = m_memText;
    snap.status = HealthEvaluator::text(
        m_health.update(cpuValid, cpuValid ? cpu.percent : 0.0, rssValid, rss));
    return snap;
}
