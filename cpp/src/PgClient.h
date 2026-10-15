#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class QTcpSocket;

// Minimal PostgreSQL v3 frontend/backend client (startup + simple query,
// text format). Sufficient for monitoring; trust auth or no-password only.
class PgClient {
public:
    PgClient();
    ~PgClient();

    bool connect(const QString &host, quint16 port, const QString &user,
                 const QString &database, int timeoutMs);
    bool query(const QString &sql, QList<QStringList> &rows);
    void close();
    bool isConnected() const;
    QString lastError() const { return m_error; }

private:
    bool readMessage(char &type, QByteArray &data, int timeoutMs);
    bool waitForBytes(int n, int timeoutMs);
    bool readUntilReady(int timeoutMs);

    QTcpSocket *m_socket = nullptr;
    QString m_error;
};
