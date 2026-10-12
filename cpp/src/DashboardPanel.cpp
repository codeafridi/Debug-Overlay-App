#include "DashboardPanel.h"

#include "MemoryFormat.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

QFont panelTitleFont() {
    QFont font;
    font.setPointSize(8);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    return font;
}

QFont monoFont(int size, bool bold = false) {
    QFont font;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("monospace"));
    font.setPointSize(size);
    font.setBold(bold);
    return font;
}

QString statusColor(const QString &status) {
    if (status == QStringLiteral("HIGH CPU")) {
        return QStringLiteral("#d1685f");
    }
    if (status == QStringLiteral("WARN")) {
        return QStringLiteral("#c9a24a");
    }
    if (status == QStringLiteral("OK")) {
        return QStringLiteral("#5fae82");
    }
    return QStringLiteral("#7f8792");
}

QLabel *makeTitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("ddCardTitle"));
    label->setFont(panelTitleFont());
    return label;
}

QFrame *makeCard(const QString &title, QWidget *content, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("ddCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    layout->addWidget(makeTitle(title, card));
    layout->addWidget(content, 1);
    return card;
}

QFrame *makeStatCard(const QString &caption, QLabel *value, QLabel *sub,
                     QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("ddStat"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(2);
    layout->addWidget(makeTitle(caption, card));
    layout->addWidget(value);
    if (sub != nullptr) {
        layout->addWidget(sub);
    }
    return card;
}

} // namespace

DashboardPanel::DashboardPanel(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("ddRoot"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *topbar = new QFrame(this);
    topbar->setObjectName(QStringLiteral("ddTopbar"));
    auto *topLayout = new QHBoxLayout(topbar);
    topLayout->setContentsMargins(16, 10, 12, 10);
    topLayout->setSpacing(10);

    auto *chip = new QLabel(QStringLiteral("SYSPEEK"), topbar);
    chip->setObjectName(QStringLiteral("ddChip"));
    chip->setFont(panelTitleFont());

    m_appValue = new QLabel(QStringLiteral("--"), topbar);
    m_appValue->setObjectName(QStringLiteral("ddApp"));
    m_appValue->setFont(monoFont(13, true));

    m_pidValue = new QLabel(QStringLiteral("pid --"), topbar);
    m_pidValue->setObjectName(QStringLiteral("ddPid"));
    m_pidValue->setFont(monoFont(9));

    m_statusValue = new QLabel(QStringLiteral("IDLE"), topbar);
    m_statusValue->setObjectName(QStringLiteral("ddStatus"));
    m_statusValue->setFont(panelTitleFont());

    auto *collapse = new QPushButton(QStringLiteral("COLLAPSE"), topbar);
    collapse->setObjectName(QStringLiteral("ddCollapse"));
    collapse->setCursor(Qt::PointingHandCursor);
    collapse->setFocusPolicy(Qt::NoFocus);
    connect(collapse, &QPushButton::clicked, this,
            &DashboardPanel::collapseRequested);

    topLayout->addWidget(chip);
    topLayout->addSpacing(4);
    topLayout->addWidget(m_appValue);
    topLayout->addWidget(m_pidValue);
    topLayout->addStretch(1);
    topLayout->addWidget(m_statusValue);
    topLayout->addWidget(collapse);
    root->addWidget(topbar);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(16, 16, 16, 16);
    bodyLayout->setSpacing(14);

    m_cpuValue = new QLabel(QStringLiteral("--"), body);
    m_cpuValue->setObjectName(QStringLiteral("ddCpu"));
    m_cpuValue->setFont(monoFont(18, true));

    m_memValue = new QLabel(QStringLiteral("--"), body);
    m_memValue->setObjectName(QStringLiteral("ddMem"));
    m_memValue->setFont(monoFont(18, true));

    m_procCount = new QLabel(QStringLiteral("0"), body);
    m_procCount->setObjectName(QStringLiteral("ddProcCount"));
    m_procCount->setFont(monoFont(18, true));

    m_statusBig = new QLabel(QStringLiteral("IDLE"), body);
    m_statusBig->setObjectName(QStringLiteral("ddStatusBig"));
    m_statusBig->setFont(monoFont(18, true));

    auto *stats = new QHBoxLayout();
    stats->setSpacing(14);
    stats->addWidget(makeStatCard(QStringLiteral("CPU"), m_cpuValue, nullptr, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("MEMORY (RSS)"), m_memValue, nullptr, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("PROCESSES"), m_procCount, nullptr, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("STATUS"), m_statusBig, nullptr, body), 1);
    bodyLayout->addLayout(stats);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);

    m_procs = new QTreeWidget(body);
    m_procs->setObjectName(QStringLiteral("ddProcs"));
    m_procs->setColumnCount(5);
    m_procs->setHeaderLabels({QStringLiteral("Process"), QStringLiteral("PID"),
                              QStringLiteral("Threads"), QStringLiteral("FDs"),
                              QStringLiteral("RSS")});
    m_procs->setRootIsDecorated(true);
    m_procs->setSelectionMode(QAbstractItemView::NoSelection);
    m_procs->setFocusPolicy(Qt::NoFocus);
    m_procs->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_procs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_procs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_procs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_procs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    connect(m_procs, &QTreeWidget::itemExpanded, this,
            [this](QTreeWidgetItem *item) {
                if (!m_populating) {
                    m_collapsedPids.remove(item->data(0, Qt::UserRole).toInt());
                }
            });
    connect(m_procs, &QTreeWidget::itemCollapsed, this,
            [this](QTreeWidgetItem *item) {
                if (!m_populating) {
                    m_collapsedPids.insert(item->data(0, Qt::UserRole).toInt());
                }
            });
    auto *left = new QVBoxLayout();
    left->setSpacing(14);
    left->addWidget(makeCard(QStringLiteral("PROCESSES"), m_procs, body), 3);

    m_net = new QLabel(QStringLiteral("--"), body);
    m_net->setObjectName(QStringLiteral("ddNet"));
    m_net->setFont(monoFont(10));
    m_net->setWordWrap(false);
    left->addWidget(makeCard(QStringLiteral("NETWORK"), m_net, body), 2);

    m_cgroup = new QLabel(QStringLiteral("--"), body);
    m_cgroup->setObjectName(QStringLiteral("ddCgroup"));
    m_cgroup->setFont(monoFont(10));
    m_cgroup->setWordWrap(true);
    left->addWidget(makeCard(QStringLiteral("CONTAINER"), m_cgroup, body), 2);

    columns->addLayout(left, 3);

    auto *right = new QVBoxLayout();
    right->setSpacing(14);

    m_memDetail = new QLabel(QStringLiteral("--"), body);
    m_memDetail->setObjectName(QStringLiteral("ddMemDetail"));
    m_memDetail->setFont(monoFont(10));
    right->addWidget(makeCard(QStringLiteral("MEMORY"), m_memDetail, body));

    m_docker = new QLabel(QStringLiteral("--"), body);
    m_docker->setObjectName(QStringLiteral("ddDocker"));
    m_docker->setFont(monoFont(10));
    m_docker->setWordWrap(true);
    right->addWidget(makeCard(QStringLiteral("DOCKER"), m_docker, body));

    m_kube = new QLabel(QStringLiteral("--"), body);
    m_kube->setObjectName(QStringLiteral("ddKube"));
    m_kube->setFont(monoFont(10));
    m_kube->setWordWrap(true);
    right->addWidget(makeCard(QStringLiteral("KUBERNETES"), m_kube, body));

    columns->addLayout(right, 2);
    bodyLayout->addLayout(columns, 1);
    root->addWidget(body, 1);

    setStyleSheet(QStringLiteral(
        "#ddRoot { background-color: #15171a; }"
        "#ddTopbar { background-color: #1b1d21; border-bottom: 1px solid #2a2d33; }"
        "#ddChip { color: #d5dae0; }"
        "#ddApp { color: #d5dae0; }"
        "#ddPid { color: #7f8792; }"
        "#ddStatus { color: #7f8792; background-color: #23262b;"
        "  padding: 3px 10px; border-radius: 3px; }"
        "#ddCollapse { color: #c4cad1; background-color: #23262b;"
        "  border: 1px solid #31353b; border-radius: 3px; padding: 4px 12px; }"
        "#ddCollapse:hover { background-color: #2a2e34; }"
        "#ddCard { background-color: #1b1d21; border: 1px solid #2a2d33;"
        "  border-radius: 4px; }"
        "#ddStat { background-color: #1b1d21; border: 1px solid #2a2d33;"
        "  border-radius: 4px; }"
        "#ddCardTitle { color: #7f8792; }"
        "#ddCpu, #ddMem, #ddProcCount, #ddStatusBig { color: #d5dae0; }"
        "#ddMemDetail { color: #c4cad1; }"
        "#ddCgroup { color: #c4cad1; }"
        "#ddDocker { color: #9fb0c4; }"
        "#ddKube { color: #b0a9c4; }"
        "QTreeWidget { background-color: transparent; border: none;"
        "  color: #c4cad1; }"
        "QTreeWidget::item { padding: 1px 0px; }"
        "QTreeWidget::item:selected { background: transparent; }"
        "QHeaderView::section { background-color: #202327; color: #7f8792;"
        "  border: none; border-bottom: 1px solid #2a2d33; padding: 4px 6px; }"));
}

void DashboardPanel::setSnapshot(const DisplaySnapshot &snapshot) {
    m_appValue->setText(snapshot.appName);
    m_pidValue->setText(QStringLiteral("pid %1").arg(snapshot.pid));
    m_cpuValue->setText(snapshot.cpu);
    m_memValue->setText(snapshot.memory);
    m_procCount->setText(QString::number(snapshot.processes.size()));

    m_statusValue->setText(snapshot.status);
    if (m_statusBig != nullptr) {
        m_statusBig->setText(snapshot.status);
        m_statusBig->setStyleSheet(
            QStringLiteral("color:%1;").arg(statusColor(snapshot.status)));
    }
    m_statusValue->setStyleSheet(
        QStringLiteral("color:%1; background-color:#1b2a22; padding:3px 10px;"
                       " border-radius:10px;")
            .arg(statusColor(snapshot.status)));

    m_memDetail->setText(QStringLiteral("RSS      %1\nVirtual  %2\nSwap     %3\nShared   %4")
                             .arg(formatMemoryKb(snapshot.rssKb),
                                  formatMemoryKb(snapshot.vmsKb),
                                  formatMemoryKb(snapshot.swapKb),
                                  formatMemoryKb(snapshot.sharedKb)));

    QString net;
    int shown = 0;
    for (const ConnectionRow &conn : snapshot.connections) {
        if (shown >= 8) {
            net += QStringLiteral("...\n");
            break;
        }
        net += QStringLiteral("%1 %2 -> %3  %4\n")
                   .arg(conn.proto, conn.local, conn.remote, conn.state);
        ++shown;
    }
    m_net->setText(net.isEmpty() ? QStringLiteral("no connections") : net);

    if (snapshot.cgroupAvailable) {
        const QString leaf = snapshot.cgroupPath.section(QLatin1Char('/'), -1);
        const QString limit =
            snapshot.cgroupMemLimit < 0
                ? QStringLiteral("unlimited")
                : formatMemoryKb(
                      static_cast<unsigned long long>(snapshot.cgroupMemLimit) / 1024);
        QString text = QStringLiteral("cgroup %1  %2\n")
                           .arg(snapshot.cgroupVersion, leaf);
        text += QStringLiteral("memory   %1 / %2\n")
                    .arg(formatMemoryKb(snapshot.cgroupMemUsage / 1024), limit);
        text += QStringLiteral("anon %1  file %2  kernel %3\n")
                    .arg(formatMemoryKb(snapshot.cgroupAnon / 1024),
                         formatMemoryKb(snapshot.cgroupFile / 1024),
                         formatMemoryKb(snapshot.cgroupKernel / 1024));
        text += QStringLiteral("cpu throttled %1 / %2 periods")
                    .arg(snapshot.cgroupThrottled)
                    .arg(snapshot.cgroupPeriods);
        m_cgroup->setText(text);
    } else {
        m_cgroup->setText(QStringLiteral("no cgroup stats"));
    }

    m_populating = true;
    m_procs->clear();
    QList<QTreeWidgetItem *> byDepth;
    QList<QTreeWidgetItem *> allItems;
    QSet<int> present;
    for (const ProcessRow &row : snapshot.processes) {
        auto *item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, row.pid);
        item->setText(0, row.name.isEmpty() ? QStringLiteral("pid %1").arg(row.pid)
                                            : row.name);
        item->setText(1, QString::number(row.pid));
        item->setText(2, row.threads > 0 ? QString::number(row.threads)
                                         : QStringLiteral("--"));
        item->setText(3, row.fds < 0 ? QStringLiteral("--")
                                     : QString::number(row.fds));
        item->setText(4, row.memory);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);

        allItems.append(item);
        present.insert(row.pid);

        if (row.depth <= 0 || byDepth.size() < row.depth) {
            m_procs->addTopLevelItem(item);
        } else {
            byDepth.at(row.depth - 1)->addChild(item);
        }
        if (byDepth.size() > row.depth) {
            byDepth.resize(row.depth);
        }
        byDepth.append(item);
    }

    // Preserve the user's expand/collapse choices across the 1s refresh.
    m_collapsedPids.intersect(present);
    for (QTreeWidgetItem *item : allItems) {
        item->setExpanded(
            !m_collapsedPids.contains(item->data(0, Qt::UserRole).toInt()));
    }
    m_populating = false;
}

void DashboardPanel::setDockerSnapshot(const DockerSnapshot &snapshot) {
    const QString summary = dockerSummaryText(snapshot);
    const QString details = dockerDetailText(snapshot);
    QString text = summary;
    if (!details.isEmpty() && details != QStringLiteral("no issues")) {
        text += QStringLiteral("\n") + details;
    }
    m_docker->setText(text.isEmpty() ? QStringLiteral("--") : text);
}

void DashboardPanel::setKubeSnapshot(const KubeSnapshot &snapshot) {
    const QString summary = kubeSummaryText(snapshot);
    const QString details = kubeDetailText(snapshot);
    QString text = summary;
    if (!details.isEmpty() && details != QStringLiteral("no issues")) {
        text += QStringLiteral("\n") + details;
    }
    m_kube->setText(text.isEmpty() ? QStringLiteral("--") : text);
}
