#pragma once

#include "DisplaySnapshot.h"
#include "DockerStatus.h"
#include "KubeStatus.h"
#include "MetricStore.h"
#include "ServicesStatus.h"

#include <QSet>
#include <QWidget>

class QLabel;
class QListWidget;
class QStackedWidget;
class QTreeWidget;
class Sparkline;

// Expanded console shown inside the HUD overlay: left rail of views, each view
// is its own grid of cards so panels never crowd each other.
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPanel(QWidget *parent = nullptr);

    void setSnapshot(const DisplaySnapshot &snapshot);
    void setDockerSnapshot(const DockerSnapshot &snapshot);
    void setKubeSnapshot(const KubeSnapshot &snapshot);
    void setServicesSnapshot(const ServicesSnapshot &snapshot);

signals:
    void collapseRequested();

private:
    QWidget *buildOverviewView();
    QWidget *buildProcessesView();
    QWidget *buildMemoryView();
    QWidget *buildNetworkView();
    QWidget *buildContainersView();
    QWidget *buildKubernetesView();
    QWidget *buildServicesView();
    QWidget *buildDependenciesView();
    QWidget *buildPlaceholderView(const QString &text);

    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memValue = nullptr;
    QLabel *m_procCount = nullptr;
    QLabel *m_statusValue = nullptr;
    QLabel *m_statusBig = nullptr;
    QLabel *m_hostCpu = nullptr;
    QLabel *m_hostMem = nullptr;
    QLabel *m_hostDisk = nullptr;
    QLabel *m_hostNet = nullptr;
    QLabel *m_hostLoad = nullptr;
    QLabel *m_memDetail = nullptr;
    QTreeWidget *m_procs = nullptr;
    QLabel *m_focusDetail = nullptr;
    QLabel *m_openFiles = nullptr;
    QLabel *m_net = nullptr;
    QLabel *m_listen = nullptr;
    QLabel *m_states = nullptr;
    QLabel *m_ifaces = nullptr;
    QLabel *m_cgroup = nullptr;
    QLabel *m_docker = nullptr;
    QLabel *m_kube = nullptr;
    QLabel *m_services = nullptr;
    QLabel *m_servicesSummary = nullptr;
    QLabel *m_deps = nullptr;

    Sparkline *m_cpuSpark = nullptr;
    Sparkline *m_memSpark = nullptr;
    Sparkline *m_hostCpuSpark = nullptr;
    MetricStore m_cpuStore{120};
    MetricStore m_memStore{120};
    MetricStore m_hostCpuStore{120};

    QListWidget *m_rail = nullptr;
    QStackedWidget *m_views = nullptr;

    QSet<int> m_collapsedPids;
    bool m_populating = false;
};
