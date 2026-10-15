#include "PathSource.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QTimer>

PathSource::PathSource(const PathConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config) {
}

void PathSource::start() {
    if (!m_config.enabled || m_config.targets.isEmpty()) {
        emit pathReady(PathSnapshot{});
        return;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("mtr")).isEmpty()) {
        emit pathReady(PathSnapshot{});
        return;
    }
    if (m_process == nullptr) {
        m_process = new QProcess(this);
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PathSource::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this,
                &PathSource::onErrorOccurred);
    }
    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setInterval(m_config.intervalMs);
        connect(m_timer, &QTimer::timeout, this, &PathSource::onTick);
    }
    m_timer->start();
    onTick();
}

void PathSource::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void PathSource::onTick() {
    if (m_running) {
        return;
    }
    m_pending = m_config.targets;
    m_reports.clear();
    m_running = true;
    runNext();
}

void PathSource::runNext() {
    if (m_pending.isEmpty()) {
        m_running = false;
        PathSnapshot snapshot;
        snapshot.available = true;
        snapshot.reports = m_reports;
        snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
        emit pathReady(snapshot);
        return;
    }
    const QString target = m_pending.takeFirst();
    m_process->start(QStringLiteral("mtr"),
                     {QStringLiteral("--report"), QStringLiteral("--json"),
                      QStringLiteral("-c"), QString::number(m_config.cycles),
                      target});
}

void PathSource::onProcessFinished(int, QProcess::ExitStatus) {
    const QByteArray output = m_process->readAllStandardOutput();
    bool ok = false;
    const QList<PathReport> reports = parseMtrJson(output, &ok);
    if (ok) {
        m_reports.append(reports);
    }
    runNext();
}

void PathSource::onErrorOccurred(QProcess::ProcessError) {
    if (m_running) {
        runNext();
    }
}
