#include "StatsdSource.h"

#include <QDateTime>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

StatsdSource::StatsdSource(const StatsdConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config) {
}

void StatsdSource::start() {
    if (!m_config.enabled || m_socket != nullptr) {
        return;
    }
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::LocalHost,
                        static_cast<quint16>(m_config.port))) {
        delete m_socket;
        m_socket = nullptr;
        emit statsdReady(StatsdSnapshot{});
        return;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &StatsdSource::onReadyRead);

    m_timer = new QTimer(this);
    m_timer->setInterval(m_config.emitIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &StatsdSource::onEmitTick);
    m_timer->start();
}

void StatsdSource::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_socket != nullptr) {
        m_socket->close();
    }
}

void StatsdSource::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(buffer.data(), buffer.size());
        m_aggregator.ingest(buffer);
    }
}

void StatsdSource::onEmitTick() {
    StatsdSnapshot snapshot = m_aggregator.snapshot();
    snapshot.sampledAtMs = QDateTime::currentMSecsSinceEpoch();
    emit statsdReady(snapshot);
}
