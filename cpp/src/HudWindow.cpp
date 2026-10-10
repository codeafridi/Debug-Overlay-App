#include "HudWindow.h"

#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
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
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
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
    m_pidValue = makeValue();
    m_cpuValue = makeValue();
    m_memoryValue = makeValue();

    metrics->addLayout(makeMetricBlock(QStringLiteral("App"), m_appValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("PID"), m_pidValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("CPU"), m_cpuValue));
    metrics->addLayout(makeMetricBlock(QStringLiteral("MEM"), m_memoryValue));
    metrics->addStretch(1);
    root->addLayout(metrics);

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
        "#caption { color:rgb(209, 187, 187); }"
        "#status  { color:rgb(187, 207, 205); }"
        "QLabel   { color:rgb(198, 205, 195); }"));

    applyPlaceholderValues(QStringLiteral("--"),
                           QStringLiteral("--"),
                           QStringLiteral("--"),
                           QStringLiteral("--"),
                           QStringLiteral("IDLE"));

    resize(460, 96);
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

void HudWindow::applyPlaceholderValues(const QString &appName,
                                       const QString &pid,
                                       const QString &cpu,
                                       const QString &memory,
                                       const QString &status) {
    m_appValue->setText(appName);
    m_pidValue->setText(pid);
    m_cpuValue->setText(cpu);
    m_memoryValue->setText(memory);
    m_statusValue->setText(status);
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
