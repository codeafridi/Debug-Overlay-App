#include "ProcessGrouper.h"

#include "LinuxProcess.h"

#include <QSet>

QList<int> ProcessGrouper::descendants(int rootPid,
                                       const QHash<int, int> &parentOf) {
    QList<int> result;
    QSet<int> inGroup;

    if (rootPid <= 0) {
        return result;
    }

    result.append(rootPid);
    inGroup.insert(rootPid);

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto it = parentOf.constBegin(); it != parentOf.constEnd(); ++it) {
            if (inGroup.contains(it.value()) && !inGroup.contains(it.key())) {
                inGroup.insert(it.key());
                result.append(it.key());
                changed = true;
            }
        }
    }

    return result;
}

ProcessGroup ProcessGrouper::discover(int focusPid) const {
    ProcessGroup group;

    if (focusPid <= 0 || !linuxproc::exists(focusPid)) {
        return group;
    }

    const QString focusedExe = linuxproc::exePath(focusPid);
    const QString focusedName = linuxproc::comm(focusPid);

    int root = focusPid;
    int current = focusPid;
    while (true) {
        const int parent = linuxproc::parentPid(current);
        if (parent <= 1) {
            break;
        }

        const QString parentExe = linuxproc::exePath(parent);
        const bool sameExe =
            !focusedExe.isEmpty() && parentExe == focusedExe;
        const bool sameName = linuxproc::comm(parent) == focusedName;

        if (!sameExe && !sameName) {
            break;
        }

        root = parent;
        current = parent;
    }

    QHash<int, int> parentOf;
    const QList<int> pids = linuxproc::allPids();
    for (int pid : pids) {
        const int parent = linuxproc::parentPid(pid);
        if (parent > 0) {
            parentOf.insert(pid, parent);
        }
    }

    group.rootPid = root;
    group.pids = descendants(root, parentOf);
    return group;
}
