#pragma once

#include "DisplaySnapshot.h"
#include "DockerStatus.h"
#include "KubeStatus.h"

#include <QSet>
#include <QWidget>

class QLabel;
class QTreeWidget;

// Expanded analysis page shown inside the HUD overlay (not a separate window).
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPanel(QWidget *parent = nullptr);

    void setSnapshot(const DisplaySnapshot &snapshot);
    void setDockerSnapshot(const DockerSnapshot &snapshot);
    void setKubeSnapshot(const KubeSnapshot &snapshot);

signals:
    void collapseRequested();

private:
    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memValue = nullptr;
    QLabel *m_procCount = nullptr;
    QLabel *m_statusValue = nullptr;
    QLabel *m_statusBig = nullptr;
    QLabel *m_memDetail = nullptr;
    QTreeWidget *m_procs = nullptr;
    QLabel *m_net = nullptr;
    QLabel *m_docker = nullptr;
    QLabel *m_kube = nullptr;
    QSet<int> m_collapsedPids;
    bool m_populating = false;
};
