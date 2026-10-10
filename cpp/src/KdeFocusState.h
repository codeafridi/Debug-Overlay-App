#pragma once

#include <QtGlobal>

#include <atomic>

struct KdeFocusState {
    std::atomic<int> pid{-1};
    std::atomic<qint64> stampMs{0};
};

KdeFocusState &kdeFocusState();
