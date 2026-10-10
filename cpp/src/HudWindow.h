#pragma once

#include <QWidget>

class QLabel;
class QKeyEvent;
class QLayout;

class HudWindow : public QWidget {
    Q_OBJECT

public:
    explicit HudWindow(QWidget *parent = nullptr);

    void applyPlaceholderValues(const QString &appName,
                                const QString &pid,
                                const QString &cpu,
                                const QString &memory,
                                const QString &status);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel *makeCaption(const QString &text);
    QLabel *makeValue();

    QLayout *makeMetricBlock(const QString &caption, QLabel *value);

    QLabel *m_appValue = nullptr;
    QLabel *m_pidValue = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memoryValue = nullptr;
    QLabel *m_statusValue = nullptr;
};
