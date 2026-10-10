#include "DockerSource.h"

#include "DockerStatus.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace {
constexpr int kIntervalMs = 5000;
constexpr int kStartTimeoutMs = 200;
constexpr int kFinishTimeoutMs = 3000;
} // namespace

DockerSource::DockerSource(QObject *parent)
    : QObject(parent) {
}

void DockerSource::start() {
    m_dockerPath = QStandardPaths::findExecutable(QStringLiteral("docker"));

    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setInterval(kIntervalMs);
        connect(m_timer, &QTimer::timeout, this, &DockerSource::sampleNow);
    }
    m_timer->start();

    sampleNow();
}

void DockerSource::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
}

void DockerSource::sampleNow() {
    if (m_dockerPath.isEmpty()) {
        emit dockerTextReady(formatDockerText(false, false, {}));
        return;
    }

    QProcess process;
    process.start(m_dockerPath,
                  {QStringLiteral("ps"), QStringLiteral("--format"),
                   QStringLiteral("{{.Status}}")});

    if (!process.waitForStarted(kStartTimeoutMs)) {
        emit dockerTextReady(formatDockerText(true, false, {}));
        return;
    }

    if (!process.waitForFinished(kFinishTimeoutMs)) {
        process.kill();
        process.waitForFinished(100);
        emit dockerTextReady(formatDockerText(true, false, {}));
        return;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        emit dockerTextReady(formatDockerText(true, false, {}));
        return;
    }

    emit dockerTextReady(
        formatDockerText(true, true, countDockerPs(process.readAllStandardOutput())));
}
