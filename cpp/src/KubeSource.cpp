#include "KubeSource.h"

#include "CostModel.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace {
constexpr qint64 kMaxOutputBytes = 1LL << 20;
} // namespace

KubeSource::KubeSource(const KubeConfig &config, const CostConfig &costConfig,
                       QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_costConfig(costConfig) {
    m_backoffMs = m_config.backoffStartMs;
}

void KubeSource::start() {
    m_kubectlPath = QStandardPaths::findExecutable(QStringLiteral("kubectl"));

    if (m_process == nullptr) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this,
                &KubeSource::onStdout);
        connect(m_process, &QProcess::readyReadStandardError, this,
                &KubeSource::onStderr);
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &KubeSource::onFinished);
        connect(m_process, &QProcess::errorOccurred, this,
                &KubeSource::onErrorOccurred);
    }

    if (m_pollTimer == nullptr) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setSingleShot(true);
        connect(m_pollTimer, &QTimer::timeout, this, &KubeSource::onPollTimeout);
    }
    if (m_deadlineTimer == nullptr) {
        m_deadlineTimer = new QTimer(this);
        m_deadlineTimer->setSingleShot(true);
        connect(m_deadlineTimer, &QTimer::timeout, this,
                &KubeSource::onDeadlineTimeout);
    }
    if (m_killTimer == nullptr) {
        m_killTimer = new QTimer(this);
        m_killTimer->setSingleShot(true);
        connect(m_killTimer, &QTimer::timeout, this, &KubeSource::onKillTimeout);
    }

    resetBackoff();
    beginQuery();
}

void KubeSource::stop() {
    m_stopping = true;

    if (m_pollTimer != nullptr) {
        m_pollTimer->stop();
    }
    if (m_deadlineTimer != nullptr) {
        m_deadlineTimer->stop();
    }
    if (m_killTimer != nullptr) {
        m_killTimer->stop();
    }

    if (m_process != nullptr && m_running) {
        m_process->terminate();
        if (!m_process->waitForFinished(m_config.killGraceMs)) {
            m_process->kill();
            m_process->waitForFinished(200);
        }
    }

    m_running = false;
    m_aborting = false;
}

void KubeSource::beginQuery() {
    if (m_stopping) {
        return;
    }

    if (m_running) {
        m_refreshPending = true;
        return;
    }

    if (m_kubectlPath.isEmpty()) {
        KubeSnapshot snapshot;
        snapshot.state = KubeState::NotConfigured;
        snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
        emit kubeReady(snapshot);
        scheduleNext(nextIntervalMs(KubeState::NotConfigured, false));
        return;
    }

    m_stdout.clear();
    m_stderr.clear();
    m_running = true;
    m_aborting = false;

    m_process->start(m_kubectlPath,
                     {QStringLiteral("get"),
                      QStringLiteral("pods,deployments,events"), QStringLiteral("-A"),
                      QStringLiteral("-o"), QStringLiteral("json")});
    m_deadlineTimer->start(m_config.timeoutMs);
}

void KubeSource::scheduleNext(int intervalMs) {
    if (m_stopping) {
        return;
    }
    if (m_refreshPending) {
        m_refreshPending = false;
        beginQuery();
        return;
    }
    m_pollTimer->start(intervalMs);
}

void KubeSource::onPollTimeout() {
    beginQuery();
}

void KubeSource::onStdout() {
    m_stdout += m_process->readAllStandardOutput();
    if (m_stdout.size() > kMaxOutputBytes) {
        m_aborting = true;
        m_abortState = KubeState::CommandFailed;
        m_abortMessage = QStringLiteral("kubectl output exceeded limit");
        m_process->terminate();
        m_killTimer->start(m_config.killGraceMs);
    }
}

void KubeSource::onStderr() {
    m_stderr += m_process->readAllStandardError();
    if (m_stderr.size() > kMaxOutputBytes) {
        m_aborting = true;
        m_abortState = KubeState::CommandFailed;
        m_abortMessage = QStringLiteral("kubectl error output exceeded limit");
        m_process->terminate();
        m_killTimer->start(m_config.killGraceMs);
    }
}

void KubeSource::onDeadlineTimeout() {
    if (!m_running) {
        return;
    }

    m_aborting = true;
    m_abortState = KubeState::TimedOut;
    m_abortMessage = QStringLiteral("kubectl command timed out");
    m_process->terminate();
    m_killTimer->start(m_config.killGraceMs);
}

void KubeSource::onKillTimeout() {
    if (m_running && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void KubeSource::onErrorOccurred(QProcess::ProcessError error) {
    if (error != QProcess::FailedToStart) {
        return;
    }

    m_deadlineTimer->stop();
    m_running = false;
    m_aborting = false;
    emitSnapshot(KubeState::CommandFailed,
                 QStringLiteral("could not start kubectl"), {}, false);
    scheduleNext(nextIntervalMs(KubeState::CommandFailed, false));
}

void KubeSource::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_deadlineTimer->stop();
    m_killTimer->stop();

    const bool wasAborting = m_aborting;
    m_running = false;
    m_aborting = false;

    if (m_stopping) {
        return;
    }

    if (wasAborting) {
        emitSnapshot(m_abortState, m_abortMessage, {}, false);
        scheduleNext(nextIntervalMs(m_abortState, false));
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const KubeState state = classifyKubeFailure(m_stderr);
        emitSnapshot(state, QString::fromUtf8(m_stderr).trimmed(), {}, false);
        scheduleNext(nextIntervalMs(state, false));
        return;
    }

    bool ok = false;
    const KubeResources resources = parseKubectlResources(m_stdout, &ok);
    if (!ok) {
        emitSnapshot(KubeState::ParseFailed,
                     QStringLiteral("unexpected kubectl output"), {}, false);
        scheduleNext(nextIntervalMs(KubeState::ParseFailed, false));
        return;
    }

    const int issues = kubeIssueCount(resources.pods, resources.deployments);
    emitSnapshot(KubeState::Ok, QString(), resources, true);
    scheduleNext(nextIntervalMs(KubeState::Ok, issues > 0));
}

void KubeSource::emitSnapshot(KubeState state, const QString &message,
                              const KubeResources &resources, bool success) {
    if (success) {
        KubeSnapshot snapshot;
        snapshot.state = KubeState::Ok;
        snapshot.pods = resources.pods;
        snapshot.deployments = resources.deployments;
        snapshot.events = resources.events;
        snapshot.totalPods = resources.pods.size();
        snapshot.issueCount =
            kubeIssueCount(resources.pods, resources.deployments);
        snapshot.eventCount = kubeWarningCount(resources.events);
        snapshot.diagnoses = kubeDiagnose(resources.pods, resources.events);
        snapshot.deprecations = checkDeprecations(resources.objects);

        const CostSummary cost =
            computeClusterCost(resources.pods, m_costConfig.cpuPerCoreHour,
                               m_costConfig.memPerGbHour);
        snapshot.costMonthly = cost.totalMonthly;
        snapshot.costMissingRequests = cost.missingRequestContainers;
        snapshot.costPods = cost.podsPriced;
        snapshot.costByNamespace = cost.byNamespace;
        snapshot.costTopWorkloads.clear();
        for (const WorkloadCost &workload : cost.topWorkloads) {
            snapshot.costTopWorkloads.append(qMakePair(
                workload.ns + QStringLiteral("/") + workload.pod,
                workload.monthly));
        }

        if (m_havePrev) {
            const QList<KubeChange> changes =
                kubeDiff(m_prevResources, resources, snapshot.sampledAtMs);
            for (const KubeChange &change : changes) {
                m_timeline.prepend(change);
            }
            while (m_timeline.size() > 200) {
                m_timeline.removeLast();
            }
        }
        snapshot.timeline = m_timeline;
        m_prevResources = resources;
        m_havePrev = true;
        snapshot.stale = false;
        snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
        m_lastGood = snapshot;
        m_haveLast = true;
        resetBackoff();
        emit kubeReady(snapshot);
        return;
    }

    if (m_haveLast) {
        KubeSnapshot snapshot = m_lastGood;
        snapshot.state = KubeState::Stale;
        snapshot.stale = true;
        snapshot.errorMessage = message;
        snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
        emit kubeReady(snapshot);
        return;
    }

    KubeSnapshot snapshot;
    snapshot.state = state;
    snapshot.errorMessage = message;
    snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
    emit kubeReady(snapshot);
}

int KubeSource::nextIntervalMs(KubeState state, bool hasIssues) {
    switch (state) {
    case KubeState::Ok:
        resetBackoff();
        return hasIssues ? m_config.issueMs : m_config.healthyMs;
    case KubeState::NotConfigured:
        return m_config.absentMs;
    case KubeState::Unreachable:
    case KubeState::TimedOut:
    case KubeState::CommandFailed:
    case KubeState::ParseFailed:
    case KubeState::Stale:
    default: {
        const int current = m_backoffMs;
        m_backoffMs = std::min(m_backoffMs * 2, m_config.backoffCapMs);
        return current;
    }
    }
}

void KubeSource::resetBackoff() {
    m_backoffMs = m_config.backoffStartMs;
}
