#pragma once

#include "Config.h"
#include "PathStatus.h"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QStringList>

class QTimer;

class PathSource : public QObject {
    Q_OBJECT

public:
    explicit PathSource(const PathConfig &config = PathConfig{},
                        QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void pathReady(const PathSnapshot &snapshot);

private slots:
    void onTick();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

private:
    void runNext();

    PathConfig m_config;
    QProcess *m_process = nullptr;
    QTimer *m_timer = nullptr;
    QStringList m_pending;
    QList<PathReport> m_reports;
    bool m_running = false;
};
