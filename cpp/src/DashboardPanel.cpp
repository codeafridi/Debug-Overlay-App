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
        return QStringLiteral("#ff6b6b");
    }
    if (status == QStringLiteral("WARN")) {
        return QStringLiteral("#f5b955");
    }
    if (status == QStringLiteral("OK")) {
        return QStringLiteral("#3fd08a");
    }
    return QStringLiteral("#8b93a1");
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
    m_procs->setColumnCount(3);
    m_procs->setHeaderLabels({QStringLiteral("Process"), QStringLiteral("PID"),
                              QStringLiteral("RSS")});
    m_procs->setRootIsDecorated(true);
    m_procs->setSelectionMode(QAbstractItemView::NoSelection);
    m_procs->setFocusPolicy(Qt::NoFocus);
    m_procs->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_procs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_procs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    columns->addWidget(makeCard(QStringLiteral("PROCESSES"), m_procs, body), 3);

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
        "#ddRoot { background-color: #101114; }"
        "#ddTopbar { background-color: #17181c; border-bottom: 1px solid #2b2d33; }"
        "#ddChip { color: #ffffff; background-color: #7a3ff2;"
        "  padding: 3px 8px; border-radius: 3px; }"
        "#ddApp { color: #e7ebf0; }"
        "#ddPid { color: #8b93a1; }"
        "#ddStatus { color: #3fd08a; background-color: #1b2a22;"
        "  padding: 3px 10px; border-radius: 10px; }"
        "#ddCollapse { color: #cfd6dd; background-color: #2a2d33;"
        "  border: 1px solid #3a3d44; border-radius: 4px; padding: 4px 12px; }"
        "#ddCollapse:hover { background-color: #34373e; }"
        "#ddCard { background-color: #17181c; border: 1px solid #2b2d33;"
        "  border-radius: 6px; }"
        "#ddStat { background-color: #17181c; border: 1px solid #2b2d33;"
        "  border-left: 3px solid #7a3ff2; border-radius: 6px; }"
        "#ddCardTitle { color: #8b93a1; }"
        "#ddCpu, #ddMem, #ddProcCount, #ddStatusBig { color: #e7ebf0; }"
        "#ddMemDetail { color: #cfd6dd; }"
        "#ddDocker { color: #8fb4e0; }"
        "#ddKube { color: #b8a2e8; }"
        "QTreeWidget { background-color: transparent; border: none;"
        "  color: #cfd6dd; }"
        "QTreeWidget::item { padding: 1px 0px; }"
        "QTreeWidget::item:selected { background: transparent; }"
        "QHeaderView::section { background-color: #1d1f24; color: #8b93a1;"
        "  border: none; border-bottom: 1px solid #2b2d33; padding: 4px 6px; }"));
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

    m_procs->clear();
    QList<QTreeWidgetItem *> byDepth;
    for (const ProcessRow &row : snapshot.processes) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, row.name.isEmpty() ? QStringLiteral("pid %1").arg(row.pid)
                                            : row.name);
        item->setText(1, QString::number(row.pid));
        item->setText(2, row.memory);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);

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
    m_procs->expandAll();
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
