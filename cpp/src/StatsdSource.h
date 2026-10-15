#pragma once

#include "Config.h"
#include "StatsdStatus.h"

#include <QObject>

class QUdpSocket;
class QTimer;

class StatsdSource : public QObject {
    Q_OBJECT

public:
    explicit StatsdSource(const StatsdConfig &config = StatsdConfig{},
                          QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void statsdReady(const StatsdSnapshot &snapshot);

private slots:
    void onReadyRead();
    void onEmitTick();

private:
    StatsdConfig m_config;
    QUdpSocket *m_socket = nullptr;
    QTimer *m_timer = nullptr;
    StatsdAggregator m_aggregator;
};
