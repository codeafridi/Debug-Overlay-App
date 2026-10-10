#pragma once

#include <QObject>

class KdeFocusService : public QObject {
    Q_OBJECT

public:
    explicit KdeFocusService(QObject *parent = nullptr);

    bool registerService();

public slots:
    void setFocusedPid(int pid);
};
