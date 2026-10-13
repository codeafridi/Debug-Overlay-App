#include "Sparkline.h"

#include <QPainter>
#include <QPolygonF>

Sparkline::Sparkline(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Sparkline::setValues(const QList<double> &values) {
    m_values = values;
    update();
}

void Sparkline::setColor(const QColor &color) {
    m_color = color;
    update();
}

void Sparkline::paintEvent(QPaintEvent *event) {
    static_cast<void>(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_values.size() < 2) {
        return;
    }

    double low = m_values.first();
    double high = m_values.first();
    for (double value : m_values) {
        low = qMin(low, value);
        high = qMax(high, value);
    }
    if (high <= low) {
        high = low + 1.0;
    }

    const double w = width() - 1.0;
    const double h = height() - 1.0;
    QPolygonF line;
    for (int i = 0; i < m_values.size(); ++i) {
        const double x = w * i / (m_values.size() - 1);
        const double y = h - (m_values.at(i) - low) / (high - low) * h;
        line << QPointF(x, y);
    }

    QPen pen(m_color);
    pen.setWidthF(1.2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(line);
}
