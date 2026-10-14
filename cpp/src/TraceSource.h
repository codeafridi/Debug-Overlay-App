#pragma once

#include "Config.h"
#include "TraceStatus.h"

#include <QHash>
#include <QObject>

class QTcpServer;
class QTcpSocket;
class QTimer;

class TraceSource : public QObject {
    Q_OBJECT

public:
    explicit TraceSource(const TraceConfig &config = TraceConfig{},
                         QObject *parent = nullptr);

    quint16 port() const;

public slots:
    void start();
    void stop();

signals:
    void tracesReady(const TraceSnapshot &snapshot);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onEmitTick();

private:
    void processBody(const QByteArray &body);
    TraceConfig m_config;
    QTcpServer *m_server = nullptr;
    QTimer *m_timer = nullptr;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<Span> m_spans;
};
