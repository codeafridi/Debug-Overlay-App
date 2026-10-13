#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class Sparkline : public QWidget {
    Q_OBJECT

public:
    explicit Sparkline(QWidget *parent = nullptr);

    void setValues(const QList<double> &values);
    void setColor(const QColor &color);

    int valueCount() const { return m_values.size(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<double> m_values;
    QColor m_color{96, 148, 200};
};
