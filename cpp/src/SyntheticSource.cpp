#include "SyntheticSource.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QTimer>

SyntheticSource::SyntheticSource(const SyntheticConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config) {
}

void SyntheticSource::start() {
    m_curlPath = QStandardPaths::findExecutable(QStringLiteral("curl"));

    if (m_process == nullptr) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this,
                [this]() { m_stdout += m_process->readAllStandardOutput(); });
        connect(m_process, &QProcess::readyReadStandardError, this,
                [this]() { m_stderr += m_process->readAllStandardError(); });
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &SyntheticSource::onFinished);
        connect(m_process, &QProcess::errorOccurred, this,
                &SyntheticSource::onErrorOccurred);
    }
    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &SyntheticSource::onTimeout);
    }

    beginRun();
}

void SyntheticSource::stop() {
    m_stopping = true;
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
            m_process->kill();
            m_process->waitForFinished(200);
        }
    }
    m_running = false;
}

void SyntheticSource::beginRun() {
    if (m_stopping) {
        return;
    }
    m_snapshot = SyntheticSnapshot{};
    m_snapshot.available = true;
    m_index = 0;
    runNext();
}

void SyntheticSource::runNext() {
    if (m_stopping) {
        return;
    }
    if (m_curlPath.isEmpty()) {
        finishRun(QStringLiteral("curl not found"));
        return;
    }
    if (m_index >= m_config.targets.size()) {
        finishRun(QString());
        return;
    }

    m_stdout.clear();
    m_stderr.clear();
    m_running = true;
    m_currentUrl = m_config.targets.at(m_index);
    const int timeoutSec = qMax(1, m_config.timeoutMs / 1000);
    m_process->start(
        m_curlPath,
        {QStringLiteral("-s"), QStringLiteral("-o"), QStringLiteral("/dev/null"),
         QStringLiteral("-w"),
         QStringLiteral("%{http_code} %{time_namelookup} %{time_connect} "
                        "%{time_appconnect} %{time_starttransfer} %{time_total}"),
         QStringLiteral("--max-time"), QString::number(timeoutSec), m_currentUrl});
}

void SyntheticSource::onFinished(int exitCode, QProcess::ExitStatus status) {
    if (!m_running) {
        return;
    }
    m_running = false;
    m_stdout += m_process->readAllStandardOutput();
    m_stderr += m_process->readAllStandardError();

    ProbeResult result;
    result.url = m_currentUrl;
    result.atMs = QDateTime::currentMSecsSinceEpoch();

    int httpStatus = 0;
    double total = 0;
    double dns = 0;
    double connect = 0;
    double tls = 0;
    double processing = 0;
    double transfer = 0;
    if (status == QProcess::NormalExit && exitCode == 0
        && parseCurlWriteOut(m_stdout, httpStatus, total, dns, connect, tls,
                             processing, transfer)) {
        result.status = httpStatus;
        result.totalMs = total;
        result.dnsMs = dns;
        result.connectMs = connect;
        result.tlsMs = tls;
        result.processingMs = processing;
        result.transferMs = transfer;
        result.success = httpStatus >= 200 && httpStatus < 400;
        if (!result.success) {
            result.error = QStringLiteral("HTTP %1").arg(httpStatus);
        }
    } else {
        result.success = false;
        result.error = QString::fromUtf8(m_stderr).trimmed();
        if (result.error.isEmpty()) {
            result.error = QStringLiteral("failed (exit %1)").arg(exitCode);
        }
    }

    m_snapshot.results.append(result);
    ++m_index;
    runNext();
}

void SyntheticSource::onErrorOccurred(QProcess::ProcessError error) {
    if (!m_running || error != QProcess::FailedToStart) {
        return;
    }
    m_running = false;
    ProbeResult result;
    result.url = m_currentUrl;
    result.success = false;
    result.error = QStringLiteral("curl failed to start");
    result.atMs = QDateTime::currentMSecsSinceEpoch();
    m_snapshot.results.append(result);
    ++m_index;
    runNext();
}

void SyntheticSource::onTimeout() {
    beginRun();
}

void SyntheticSource::finishRun(const QString &error) {
    m_snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
    if (!error.isEmpty() && m_snapshot.results.isEmpty()) {
        ProbeResult result;
        result.url = QStringLiteral("(curl)");
        result.success = false;
        result.error = error;
        m_snapshot.results.append(result);
    }

    m_snapshot.upCount = 0;
    m_snapshot.downCount = 0;
    for (const ProbeResult &result : m_snapshot.results) {
        if (result.success) {
            ++m_snapshot.upCount;
        } else {
            ++m_snapshot.downCount;
        }
    }

    emit syntheticReady(m_snapshot);
    if (m_timer != nullptr) {
        m_timer->start(m_config.intervalMs);
    }
}
