#pragma once

#include "ProcessIdentity.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

namespace linuxproc {

struct ProcEntry {
    int pid = -1;
    int ppid = -1;
    unsigned long long startTime = 0;
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    unsigned long long rssKb = 0;
    QString comm;
};

// True when /proc must be read through the host (for example inside a Flatpak
// sandbox, where the app has a private PID namespace).
bool hostProcEnabled();

// Force a fresh /proc snapshot at the start of a sampling pass. A no-op outside
// a sandbox. Within one pass every read reuses the same snapshot so CPU deltas
// stay internally consistent.
void beginSample();

// Parse the compact host /proc dump produced by the host sampler. Exposed for
// testing. Sums the aggregate CPU line into totalTicks.
QHash<int, ProcEntry> parseHostProcDump(const QByteArray &output,
                                        unsigned long long &totalTicks);

bool exists(int pid);
QString comm(int pid);
QString exePath(int pid);
int parentPid(int pid);
QList<int> allPids();
unsigned long long startTime(int pid);
ProcessIdentity identity(int pid);

bool cpuTicks(int pid, unsigned long long &out);
bool totalCpuTicks(unsigned long long &out);
bool rssKb(int pid, unsigned long long &out);
bool sumRssKb(const QList<int> &pids, unsigned long long &out);

} // namespace linuxproc
