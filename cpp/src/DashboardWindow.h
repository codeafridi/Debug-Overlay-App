#pragma once

#include "DisplaySnapshot.h"
#include "DockerStatus.h"
#include "KubeStatus.h"

#include <QWidget>

class QLabel;
class QTreeWidget;

class DashboardWindow : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWindow(QWidget *parent = nullptr);

    void setSnapshot(const DisplaySnapshot &snapshot);
    void setDockerSnapshot(const DockerSnapshot &snapshot);
    void setKubeSnapshot(const KubeSnapshot &snapshot);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memValue = nullptr;
    QLabel *m_statusValue = nullptr;
    QLabel *m_memDetail = nullptr;
    QTreeWidget *m_procs = nullptr;
    QLabel *m_docker = nullptr;
    QLabel *m_kube = nullptr;
};
