#include "DashboardWindow.h"

#include "MemoryFormat.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWindow>

namespace {

QFont cardTitleFont() {
    QFont font;
    font.setPointSize(9);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    return font;
}

QFont valueFont() {
    QFont font;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("monospace"));
    font.setPointSize(10);
    font.setBold(true);
    return font;
}

QFont bigValueFont() {
    QFont font = valueFont();
    font.setPointSize(15);
    return font;
}

QString statusColor(const QString &status) {
    if (status == QStringLiteral("HIGH CPU")) {
        return QStringLiteral("#ff6b6b");
    }
    if (status == QStringLiteral("WARN")) {
        return QStringLiteral("#ffd45d");
    }
    if (status == QStringLiteral("OK")) {
        return QStringLiteral("#79ffb0");
    }
    return QStringLiteral("#bbcfcd");
}

QLabel *makeTitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("cardTitle"));
    label->setFont(cardTitleFont());
    return label;
}

QFrame *makeCard(const QString &title, QWidget *content, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);
    layout->addWidget(makeTitle(title, card));
    layout->addWidget(content, 1);
    return card;
}

QFrame *makeStatCard(const QString &caption, QWidget *value, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(4);
    layout->addWidget(makeTitle(caption, card));
    layout->addWidget(value);
    return card;
}

} // namespace

DashboardWindow::DashboardWindow(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("dashRoot"));
    setWindowTitle(QStringLiteral("Syspeek"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(760, 520);
    resize(960, 660);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("dashHeader"));
    header->installEventFilter(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 8, 10, 8);
    headerLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("SYSPEEK"), header);
    title->setObjectName(QStringLiteral("dashTitle"));
    title->setFont(cardTitleFont());

    auto *subtitle = new QLabel(QStringLiteral("focused application"), header);
    subtitle->setObjectName(QStringLiteral("dashSubtitle"));

    auto *closeButton = new QPushButton(QStringLiteral("\u2715"), header);
    closeButton->setObjectName(QStringLiteral("dashClose"));
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFocusPolicy(Qt::NoFocus);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);

    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(header);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(16, 16, 16, 16);
    bodyLayout->setSpacing(14);

    m_appValue = new QLabel(QStringLiteral("--"), body);
    m_appValue->setObjectName(QStringLiteral("dashApp"));
    m_appValue->setFont(bigValueFont());
    m_pidValue = new QLabel(QStringLiteral("pid --"), body);
    m_pidValue->setObjectName(QStringLiteral("dashPid"));
    m_pidValue->setFont(valueFont());

    m_cpuValue = new QLabel(QStringLiteral("--"), body);
    m_cpuValue->setObjectName(QStringLiteral("dashCpu"));
    m_cpuValue->setFont(bigValueFont());

    m_memValue = new QLabel(QStringLiteral("--"), body);
    m_memValue->setObjectName(QStringLiteral("dashMem"));
    m_memValue->setFont(bigValueFont());

    m_statusValue = new QLabel(QStringLiteral("IDLE"), body);
    m_statusValue->setObjectName(QStringLiteral("dashStatus"));
    m_statusValue->setFont(bigValueFont());

    auto *appContent = new QWidget(body);
    auto *appLayout = new QVBoxLayout(appContent);
    appLayout->setContentsMargins(0, 0, 0, 0);
    appLayout->setSpacing(2);
    appLayout->addWidget(m_appValue);
    appLayout->addWidget(m_pidValue);

    auto *stats = new QHBoxLayout();
    stats->setSpacing(14);
    stats->addWidget(makeStatCard(QStringLiteral("APPLICATION"), appContent, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("CPU"), m_cpuValue, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("MEMORY (RSS)"), m_memValue, body), 1);
    stats->addWidget(makeStatCard(QStringLiteral("STATUS"), m_statusValue, body), 1);
    bodyLayout->addLayout(stats);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);

    m_procs = new QTreeWidget(body);
    m_procs->setObjectName(QStringLiteral("dashProcs"));
    m_procs->setColumnCount(3);
    m_procs->setHeaderLabels({QStringLiteral("Process"), QStringLiteral("PID"),
                              QStringLiteral("RSS")});
    m_procs->setRootIsDecorated(true);
    m_procs->setAlternatingRowColors(false);
    m_procs->setSelectionMode(QAbstractItemView::NoSelection);
    m_procs->setFocusPolicy(Qt::NoFocus);
    m_procs->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_procs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_procs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    columns->addWidget(makeCard(QStringLiteral("PROCESSES"), m_procs, body), 3);

    auto *right = new QVBoxLayout();
    right->setSpacing(14);

    m_memDetail = new QLabel(QStringLiteral("--"), body);
    m_memDetail->setObjectName(QStringLiteral("dashMemDetail"));
    m_memDetail->setFont(valueFont());
    right->addWidget(makeCard(QStringLiteral("MEMORY BREAKDOWN"), m_memDetail, body));

    m_docker = new QLabel(QStringLiteral("--"), body);
    m_docker->setObjectName(QStringLiteral("dashDocker"));
    m_docker->setFont(valueFont());
    m_docker->setWordWrap(true);
    right->addWidget(makeCard(QStringLiteral("DOCKER"), m_docker, body));

    m_kube = new QLabel(QStringLiteral("--"), body);
    m_kube->setObjectName(QStringLiteral("dashKube"));
    m_kube->setFont(valueFont());
    m_kube->setWordWrap(true);
    right->addWidget(makeCard(QStringLiteral("KUBERNETES"), m_kube, body));

    columns->addLayout(right, 2);
    bodyLayout->addLayout(columns, 1);

    root->addWidget(body, 1);

    setStyleSheet(QStringLiteral(
        "#dashRoot { background-color: #131314; }"
        "#dashHeader { background-color: #1c1c1e; border-bottom: 1px solid #2a2a2e; }"
        "#dashTitle { color: #f4f8ff; background-color: #7c8086; padding: 3px 8px; }"
        "#dashSubtitle { color: #7f8892; }"
        "#dashClose { color: #cfd6dd; background: transparent; border: none;"
        "  font-size: 14px; padding: 2px 8px; }"
        "#dashClose:hover { color: #ff6b6b; }"
        "#card { background-color: #1c1c1e; border: 1px solid #33333a;"
        "  border-radius: 8px; }"
        "#cardTitle { color: #8a939d; }"
        "#dashApp { color: #e6edf3; }"
        "#dashPid { color: #7f8892; }"
        "#dashCpu { color: #e6edf3; }"
        "#dashMem { color: #e6edf3; }"
        "#dashMemDetail { color: #cfd6dd; }"
        "#dashDocker { color: #96b4dc; }"
        "#dashKube { color: #beaae6; }"
        "QTreeWidget { background-color: transparent; border: none;"
        "  color: #cfd6dd; }"
        "QTreeWidget::item { padding: 2px 0px; }"
        "QHeaderView::section { background-color: transparent; color: #8a939d;"
        "  border: none; padding: 2px 4px; }"));
}

bool DashboardWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && windowHandle() != nullptr
            && windowHandle()->startSystemMove()) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DashboardWindow::setSnapshot(const DisplaySnapshot &snapshot) {
    m_appValue->setText(snapshot.appName);
    m_pidValue->setText(QStringLiteral("pid %1").arg(snapshot.pid));
    m_cpuValue->setText(snapshot.cpu);
    m_memValue->setText(snapshot.memory);
    m_statusValue->setText(snapshot.status);
    m_statusValue->setStyleSheet(
        QStringLiteral("color:%1;").arg(statusColor(snapshot.status)));

    m_memDetail->setText(QStringLiteral("RSS    %1\nVMS    %2\nSwap   %3\nShared %4")
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

void DashboardWindow::setDockerSnapshot(const DockerSnapshot &snapshot) {
    const QString summary = dockerSummaryText(snapshot);
    const QString details = dockerDetailText(snapshot);
    QString text = summary;
    if (!details.isEmpty() && details != QStringLiteral("no issues")) {
        text += QStringLiteral("\n") + details;
    }
    m_docker->setText(text.isEmpty() ? QStringLiteral("--") : text);
}

void DashboardWindow::setKubeSnapshot(const KubeSnapshot &snapshot) {
    const QString summary = kubeSummaryText(snapshot);
    const QString details = kubeDetailText(snapshot);
    QString text = summary;
    if (!details.isEmpty() && details != QStringLiteral("no issues")) {
        text += QStringLiteral("\n") + details;
    }
    m_kube->setText(text.isEmpty() ? QStringLiteral("--") : text);
}
