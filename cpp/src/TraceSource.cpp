#include "TraceSource.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>

TraceSource::TraceSource(const TraceConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config) {
}

quint16 TraceSource::port() const {
    return m_server != nullptr ? m_server->serverPort() : 0;
}

void TraceSource::start() {
    if (!m_config.enabled || m_server != nullptr) {
        return;
    }
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost,
                          static_cast<quint16>(m_config.port))) {
        delete m_server;
        m_server = nullptr;
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this,
            &TraceSource::onNewConnection);

    m_timer = new QTimer(this);
    m_timer->setInterval(m_config.emitIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &TraceSource::onEmitTick);
    m_timer->start();
}

void TraceSource::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_server != nullptr) {
        m_server->close();
    }
}

void TraceSource::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &TraceSource::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket,
                &QObject::deleteLater);
    }
}

void TraceSource::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr || !m_buffers.contains(socket)) {
        return;
    }
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }
    const QByteArray headers = buffer.left(headerEnd);
    int contentLength = 0;
    for (const QByteArray &line : headers.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:")) {
            contentLength = trimmed.mid(15).trimmed().toInt();
        }
    }
    const QByteArray body = buffer.mid(headerEnd + 4);
    if (contentLength > 0 && body.size() < contentLength) {
        return; // wait for the rest
    }

    if (contentLength > 0) {
        processBody(body.left(contentLength));
    }

    const QByteArray response =
        QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                          "Content-Length: 2\r\nConnection: close\r\n\r\n{}");
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
    m_buffers.remove(socket);
}

void TraceSource::processBody(const QByteArray &body) {
    bool ok = false;
    const QList<Span> spans = parseOtlpTraces(body, &ok);
    if (!ok) {
        return;
    }
    m_spans.append(spans);
    if (m_spans.size() > m_config.maxSpans) {
        m_spans.erase(m_spans.begin(),
                      m_spans.begin() + (m_spans.size() - m_config.maxSpans));
    }
}

void TraceSource::onEmitTick() {
    emit tracesReady(summarizeTraces(m_spans));
}
