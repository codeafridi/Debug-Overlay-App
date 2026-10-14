#pragma once

#include "TcpStatus.h"

#include <QElapsedTimer>

#include <functional>

struct TcpSource {
    std::function<bool(QByteArray &)> snmp;
    std::function<bool(QByteArray &)> netstat;

    static TcpSource system();
};

class TcpSampler {
public:
    explicit TcpSampler(TcpSource source = TcpSource::system());

    TcpHealth sample();
    void reset();

private:
    TcpSource m_source;
    bool m_hasBaseline = false;
    TcpCounters m_last;
    QElapsedTimer m_timer;
};
