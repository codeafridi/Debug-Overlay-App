#include "HudWindow.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFont valueFont() {
    QFont font;
    font.setStyleHint(QFont::Monospace);
    font.setFamily(QStringLiteral("monospace"));
    font.setPointSize(10);
    font.setBold(true);
    return font;
}

QFont captionFont() {
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    return font;
}

} // namespace

HudWindow::HudWindow(QWidget *parent)
    : QWidget(parent) {
    setWindowTitle(QStringLiteral("Debug Overlay"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_X11DoNotAcceptFocus, true);
    setObjectName(QStringLiteral("hudRoot"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 16);
    root->setSpacing(10);

    auto *header = new QHBoxLayout();
    header->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("DEBUG HUD"), this);
    title->setObjectName(QStringLiteral("title"));
    title->setFont(captionFont());

    m_statusValue = makeValue();
    m_statusValue->setObjectName(QStringLiteral("status"));

    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(m_statusValue);
    root->addLayout(header);

    auto *metrics = new QHBoxLayout();
    metrics->setSpacing(18);

    m_appValue = makeValue();
    m_appValue->setObjectName(QStringLiteral("appValue"));
    m_pidValue = makeValue();
    m_pidValue->setObjectName(QStringLiteral("pidValue"));
    m_cpuValue = makeValue();
    m_cpuValue->setObjectName(QStringLiteral("cpuValue"));
    m_memoryValue = makeValue();
    m_memoryValue->setObjectName(QStringLiteral("memoryValue"));

    metrics->addLayout(makeMetricBlock(QStringLiteral("App"), m_appValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("PID"), m_pidValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("CPU"), m_cpuValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("MEM (RSS)"), m_memoryValue));
    metrics->addStretch(1);

    m_metricsWidget = new QWidget(this);
    m_metricsWidget->setObjectName(QStringLiteral("metrics"));
    m_metricsWidget->setLayout(metrics);
    root->addWidget(m_metricsWidget);

    m_compactLabel = new QLabel(this);
    m_compactLabel->setObjectName(QStringLiteral("compact"));
    m_compactLabel->setFont(valueFont());
    m_compactLabel->setVisible(false);
    root->addWidget(m_compactLabel);

    m_detailsPanel = new QFrame(this);
    m_detailsPanel->setObjectName(QStringLiteral("detailsPanel"));

    auto *detailsLayout = new QVBoxLayout(m_detailsPanel);
    detailsLayout->setContentsMargins(10, 8, 10, 10);
    detailsLayout->setSpacing(1);

    auto *detailsTitle = new QLabel(QStringLiteral("DIAGNOSIS"), m_detailsPanel);
    detailsTitle->setObjectName(QStringLiteral("caption"));
    detailsTitle->setFont(captionFont());

    m_detailsText = new QLabel(m_detailsPanel);
    m_detailsText->setObjectName(QStringLiteral("details"));
    m_detailsText->setWordWrap(true);
    m_detailsText->setText(QStringLiteral(
        "No issues detected.\n"
        "MEM (RSS) is the sum of resident memory (RSS) across the app's "
        "process group; shared pages may be counted more than once, so it is "
        "not uniquely owned physical memory."));

    detailsLayout->addWidget(detailsTitle);
    detailsLayout->addWidget(m_detailsText);
    m_detailsPanel->setVisible(false);
    root->addWidget(m_detailsPanel);

    auto *footer = new QHBoxLayout();
    footer->setSpacing(1);
    footer->addStretch(1);

    m_freezeButton = new QPushButton(QStringLiteral("FRZ"), this);
    m_freezeButton->setObjectName(QStringLiteral("freezeButton"));
    m_freezeButton->setFocusPolicy(Qt::NoFocus);
    m_freezeButton->setCursor(Qt::PointingHandCursor);
    connect(m_freezeButton, &QPushButton::clicked, this, &HudWindow::toggleFreeze);

    m_moreButton = new QPushButton(QStringLiteral("MORE"), this);
    m_moreButton->setObjectName(QStringLiteral("moreButton"));
    m_moreButton->setFocusPolicy(Qt::NoFocus);
    m_moreButton->setCursor(Qt::PointingHandCursor);
    connect(m_moreButton, &QPushButton::clicked, this, &HudWindow::toggleDetails);

    m_showHideButton = new QPushButton(QStringLiteral("HIDE"), this);
    m_showHideButton->setObjectName(QStringLiteral("showHideButton"));
    m_showHideButton->setFocusPolicy(Qt::NoFocus);
    m_showHideButton->setCursor(Qt::PointingHandCursor);
    connect(m_showHideButton, &QPushButton::clicked, this, &HudWindow::toggleCompact);

    footer->addWidget(m_freezeButton);
    footer->addWidget(m_moreButton);
    footer->addWidget(m_showHideButton);
    root->addLayout(footer);

    setStyleSheet(QStringLiteral(
        "#hudRoot {"
        "  background-color:rgb(19, 19, 20);"
        "  border: 2px solidrgb(13, 13, 14);"
        "}"
        "#title {"
        "  color: #f4f8ff;"
        "  background-color:rgb(124, 128, 134);"
        "  padding: 3px 8px;"
        "}"
        "#caption { color:rgb(200, 194, 194); }"
        "#status  { color:rgb(187, 207, 205); }"
        "#detailsPanel {"
        "  background-color:rgb(28, 28, 30);"
        "  border: 1px solid rgb(70, 70, 74);"
        "}"
        "#freezeButton, #moreButton, #showHideButton {"
        "  color: #f4f8ff;"
        "  background-color:rgb(70, 74, 82);"
        "  border: 1px solid rgb(120, 124, 132);"
        "  padding: 2px 8px;"
        "}"
        "#freezeButton:hover, #moreButton:hover, #showHideButton:hover { background-color:rgb(96, 100, 110); }"
        "QLabel   { color:rgb(198, 205, 195); }"));

    setSnapshot(DisplaySnapshot{});

    setFixedWidth(440);
    adjustSize();
}

QLabel *HudWindow::makeCaption(const QString &text) {
    auto *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("caption"));
    label->setFont(captionFont());
    return label;
}

QLabel *HudWindow::makeValue() {
    auto *label = new QLabel(QStringLiteral("--"), this);
    label->setFont(valueFont());
    return label;
}

QLayout *HudWindow::makeMetricBlock(const QString &caption, QLabel *value) {
    auto *box = new QVBoxLayout();
    box->setSpacing(2);
    box->addWidget(makeCaption(caption));
    box->addWidget(value);
    return box;
}

void HudWindow::setSnapshot(const DisplaySnapshot &snapshot) {
    if (m_frozen) {
        return;
    }

    m_lastSnapshot = snapshot;

    if (m_appValue->text() != snapshot.appName) {
        m_appValue->setText(snapshot.appName);
    }
    if (m_pidValue->text() != snapshot.pid) {
        m_pidValue->setText(snapshot.pid);
    }
    if (m_cpuValue->text() != snapshot.cpu) {
        m_cpuValue->setText(snapshot.cpu);
    }
    if (m_memoryValue->text() != snapshot.memory) {
        m_memoryValue->setText(snapshot.memory);
    }
    if (m_statusValue->text() != snapshot.status) {
        m_statusValue->setText(snapshot.status);
    }

    updateCompactText();
}

void HudWindow::updateCompactText() {
    if (m_compactLabel == nullptr) {
        return;
    }

    m_compactLabel->setText(QStringLiteral("%1  |  PID %2  |  CPU %3  |  MEM (RSS) %4  |  %5")
                                .arg(m_appValue->text(),
                                     m_pidValue->text(),
                                     m_cpuValue->text(),
                                     m_memoryValue->text(),
                                     m_statusValue->text()));
}

void HudWindow::toggleDetails() {
    m_detailsExpanded = !m_detailsExpanded;
    m_detailsPanel->setVisible(m_detailsExpanded);
    m_moreButton->setText(m_detailsExpanded ? QStringLiteral("LESS")
                                            : QStringLiteral("MORE"));
    adjustSize();
}

void HudWindow::toggleCompact() {
    m_compact = !m_compact;

    m_metricsWidget->setVisible(!m_compact);
    m_compactLabel->setVisible(m_compact);
    m_showHideButton->setText(m_compact ? QStringLiteral("SHOW")
                                        : QStringLiteral("HIDE"));

    if (m_compact) {
        m_detailsExpanded = false;
        m_detailsPanel->setVisible(false);
        m_moreButton->setText(QStringLiteral("MORE"));
        m_moreButton->setVisible(false);
    } else {
        m_moreButton->setVisible(true);
    }

    adjustSize();
}

void HudWindow::toggleFreeze() {
    m_frozen = !m_frozen;
    m_freezeButton->setText(m_frozen ? QStringLiteral("RES")
                                     : QStringLiteral("FRZ"));
}

void HudWindow::keyPressEvent(QKeyEvent *event) {
    const bool quitChord =
        event->key() == Qt::Key_Q &&
        event->modifiers().testFlag(Qt::ControlModifier);

    if (event->key() == Qt::Key_Escape || quitChord) {
        close();
        return;
    }

    QWidget::keyPressEvent(event);
}

void HudWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset =
            event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void HudWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void HudWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}
