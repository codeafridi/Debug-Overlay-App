#pragma once

#include <QList>

// Fixed-capacity ring of recent samples for sparklines.
class MetricStore {
public:
    explicit MetricStore(int capacity = 120);

    void push(double value);
    void clear();

    int size() const;
    int capacity() const;
    double last() const;
    double minimum() const;
    double maximum() const;

    const QList<double> &values() const;

private:
    QList<double> m_values;
    int m_capacity;
};
