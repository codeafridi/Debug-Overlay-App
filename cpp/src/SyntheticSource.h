#pragma once

#include "Config.h"
#include "SyntheticStatus.h"

#include <QObject>
#include <QProcess>
#include <QString>

class QTimer;

class SyntheticSource : public QObject {
    Q_OBJECT

public:
    explicit SyntheticSource(const SyntheticConfig &config, QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void syntheticReady(const SyntheticSnapshot &snapshot);

private slots:
    void onTimeout();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

private:
    void beginRun();
    void runNext();
    void finishRun(const QString &error);

    QProcess *m_process = nullptr;
    QTimer *m_timer = nullptr;
    SyntheticConfig m_config;
    QString m_curlPath;
    QByteArray m_stdout;
    QByteArray m_stderr;
    SyntheticSnapshot m_snapshot;
    QString m_currentUrl;
    int m_index = 0;
    bool m_running = false;
    bool m_stopping = false;
};
