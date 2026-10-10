#pragma once

#include <QHash>
#include <QList>

struct ProcessGroup {
    int rootPid = -1;
    QList<int> pids;
};

class ProcessGrouper {
public:
    static QList<int> descendants(int rootPid,
                                  const QHash<int, int> &parentOf);

    ProcessGroup discover(int focusPid) const;
};
