#pragma once

#include "DisplaySnapshot.h"
#include "DockerStatus.h"
#include "JournalStatus.h"
#include "KubeStatus.h"
#include "MetricStore.h"
#include "ServicesStatus.h"
#include "AlertEngine.h"
#include "SyntheticStatus.h"

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
    void setJournalSnapshot(const JournalSnapshot &snapshot);
    void setAlerts(const QList<Alert> &alerts);
    void setSyntheticSnapshot(const SyntheticSnapshot &snapshot);

signals:
    void collapseRequested();

private:
    QWidget *buildOverviewView();
    QWidget *buildAlertsView();
    QWidget *buildProcessesView();
    QWidget *buildMemoryView();
    QWidget *buildNetworkView();
    QWidget *buildContainersView();
    QWidget *buildKubernetesView();
    QWidget *buildServicesView();
    QWidget *buildDependenciesView();
    QWidget *buildLogsView();
    QWidget *buildErrorsView();
    QWidget *buildEndpointsView();
    QWidget *buildSyntheticView();
    QWidget *buildInstrumentationView();
    QWidget *buildDatabasesView();
    QWidget *buildPlaceholderView(const QString &text);
    QWidget *buildCostView();
    QWidget *buildDisksView();
    QWidget *buildSecurityView();

    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_alerts = nullptr;
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
    QLabel *m_tcpSummary = nullptr;
    QLabel *m_tcp = nullptr;
    QLabel *m_cgroup = nullptr;
    QLabel *m_docker = nullptr;
    QLabel *m_kubeSummary = nullptr;
    QTreeWidget *m_kubePods = nullptr;
    QTreeWidget *m_kubeDeploys = nullptr;
    QLabel *m_kubeEvents = nullptr;
    QLabel *m_kubeDiagnosis = nullptr;
    QLabel *m_kubeTimeline = nullptr;
    QLabel *m_costSummary = nullptr;
    QLabel *m_costWarnings = nullptr;
    QTreeWidget *m_costNamespaces = nullptr;
    QTreeWidget *m_costWorkloads = nullptr;
    QLabel *m_diskSummary = nullptr;
    QLabel *m_diskPressure = nullptr;
    QTreeWidget *m_disks = nullptr;
    QLabel *m_securitySummary = nullptr;
    QLabel *m_security = nullptr;
    QLabel *m_services = nullptr;
    QLabel *m_servicesSummary = nullptr;
    QLabel *m_servicesChanges = nullptr;
    QLabel *m_deps = nullptr;
    QLabel *m_logs = nullptr;
    QLabel *m_logsSummary = nullptr;
    QLabel *m_logPipeline = nullptr;
    QLabel *m_errors = nullptr;
    QLabel *m_endpoints = nullptr;
    QLabel *m_synthetic = nullptr;
    QLabel *m_syntheticSummary = nullptr;
    QLabel *m_instrumentation = nullptr;
    QLabel *m_databases = nullptr;

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
