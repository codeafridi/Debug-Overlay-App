#pragma once

struct ProcessIdentity {
    int pid = -1;
    unsigned long long startTime = 0;

    bool isValid() const { return pid > 0 && startTime > 0; }

    bool operator==(const ProcessIdentity &other) const {
        return pid == other.pid && startTime == other.startTime;
    }

    bool operator!=(const ProcessIdentity &other) const {
        return !(*this == other);
    }
};
