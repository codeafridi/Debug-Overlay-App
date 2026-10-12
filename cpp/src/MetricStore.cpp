#include "MetricStore.h"

MetricStore::MetricStore(int capacity)
    : m_capacity(capacity > 1 ? capacity : 1) {
}

void MetricStore::push(double value) {
    m_values.append(value);
    while (m_values.size() > m_capacity) {
        m_values.removeFirst();
    }
}

void MetricStore::clear() {
    m_values.clear();
}

int MetricStore::size() const {
    return m_values.size();
}

int MetricStore::capacity() const {
    return m_capacity;
}

double MetricStore::last() const {
    return m_values.isEmpty() ? 0.0 : m_values.last();
}

double MetricStore::minimum() const {
    if (m_values.isEmpty()) {
        return 0.0;
    }
    double value = m_values.first();
    for (double v : m_values) {
        if (v < value) {
            value = v;
        }
    }
    return value;
}

double MetricStore::maximum() const {
    if (m_values.isEmpty()) {
        return 0.0;
    }
    double value = m_values.first();
    for (double v : m_values) {
        if (v > value) {
            value = v;
        }
    }
    return value;
}

const QList<double> &MetricStore::values() const {
    return m_values;
}
