#pragma once

#include "JournalStatus.h"

#include <QObject>
#include <QProcess>

class QTimer;

class JournalSource : public QObject {
    Q_OBJECT

public:
    explicit JournalSource(int intervalMs = 10000, QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void journalReady(const JournalSnapshot &snapshot);

private slots:
    void onTimeout();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

private:
    void beginQuery();
    void emitFailure(const QString &message);

    QProcess *m_process = nullptr;
    QTimer *m_timer = nullptr;
    QByteArray m_stdout;
    int m_intervalMs;
    bool m_running = false;
    bool m_stopping = false;
};
