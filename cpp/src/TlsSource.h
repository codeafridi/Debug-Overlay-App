#pragma once

#include "Config.h"
#include "TlsStatus.h"

#include <QObject>

class QTimer;

class TlsSource : public QObject {
    Q_OBJECT

public:
    explicit TlsSource(const TlsConfig &config = TlsConfig{},
                       QObject *parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void tlsReady(const TlsSnapshot &snapshot);

private slots:
    void onTick();

private:
    TlsConfig m_config;
    QTimer *m_timer = nullptr;
};
