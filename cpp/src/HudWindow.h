#pragma once

#include "DisplaySnapshot.h"

#include <QPoint>
#include <QWidget>

class QLabel;
class QKeyEvent;
class QLayout;
class QMouseEvent;
class QPushButton;

class HudWindow : public QWidget {
    Q_OBJECT

public:
    explicit HudWindow(QWidget *parent = nullptr);

    void setSnapshot(const DisplaySnapshot &snapshot);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *makeCaption(const QString &text);
    QLabel *makeValue();

    QLayout *makeMetricBlock(const QString &caption, QLabel *value);

    void toggleDetails();
    void toggleCompact();
    void toggleFreeze();
    void updateCompactText();

    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memoryValue = nullptr;
    QLabel *m_statusValue = nullptr;

    QWidget *m_metricsWidget = nullptr;
    QLabel *m_compactLabel = nullptr;
    QWidget *m_detailsPanel = nullptr;
    QLabel *m_detailsText = nullptr;
    QPushButton *m_moreButton = nullptr;
    QPushButton *m_showHideButton = nullptr;
    QPushButton *m_freezeButton = nullptr;
    DisplaySnapshot m_lastSnapshot;
    bool m_detailsExpanded = false;
    bool m_compact = false;
    bool m_frozen = false;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
