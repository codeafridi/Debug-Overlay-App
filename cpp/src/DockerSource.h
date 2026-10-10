#pragma once

#include <QObject>
#include <QString>

class QTimer;

class DockerSource : public QObject {
    Q_OBJECT

public:
    explicit DockerSource(QObject *parent = nullptr);

public slots:
    void start();
    void stop();
    void sampleNow();

signals:
    void dockerTextReady(const QString &text);

private:
    QTimer *m_timer = nullptr;
    QString m_dockerPath;
};
