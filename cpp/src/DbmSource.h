#pragma once

#include "Config.h"
#include "DbmStatus.h"
#include "PgClient.h"

#include <QElapsedTimer>
#include <QObject>

class QTimer;

class DbmSource : public QObject {
    Q_OBJECT

public:
    explicit DbmSource(const DbmConfig &config = DbmConfig{},
                       QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void dbmReady(const DbmSnapshot &snapshot);

private slots:
    void onTick();

private:
    DbmConfig m_config;
    QTimer *m_timer = nullptr;
    PgClient m_pg;
    bool m_haveBaseline = false;
    quint64 m_lastCommit = 0;
    QElapsedTimer m_rateTimer;
};
