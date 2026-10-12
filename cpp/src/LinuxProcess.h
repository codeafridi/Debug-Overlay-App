#pragma once

#include "ProcessIdentity.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>

namespace linuxproc {

struct MemInfo {
    unsigned long long rssKb = 0;
    unsigned long long vmsKb = 0;
    unsigned long long swapKb = 0;
    unsigned long long sharedKb = 0;
};

struct ProcDetail {
    int threads = 0;
    unsigned long long voluntaryCtx = 0;
    unsigned long long involuntaryCtx = 0;
    int fdCount = -1;
};

bool exists(int pid);
QString comm(int pid);
QString exePath(int pid);
int parentPid(int pid);
QList<int> allPids();
unsigned long long startTime(int pid);
ProcessIdentity identity(int pid);

bool cpuTicks(int pid, unsigned long long &out);
bool procCpu(int pid, unsigned long long &ticks, unsigned long long &startTime);
bool totalCpuTicks(unsigned long long &out);
int onlineCpuCount();
bool rssKb(int pid, unsigned long long &out);
bool parseVmRssKb(const QByteArray &statusContent, unsigned long long &out);
bool parseStatusMem(const QByteArray &statusContent, MemInfo &out);
bool parseStatmShared(const QByteArray &statmContent, unsigned long long pageSize,
                      unsigned long long &sharedKb);
bool procMem(int pid, MemInfo &out);
bool procSample(int pid, MemInfo &mem, ProcDetail &detail);
bool parseStatusDetail(const QByteArray &statusContent, ProcDetail &out);
int countFds(int pid);
using RssReader = std::function<bool(int, unsigned long long &)>;
bool sumRssKbWith(const QList<int> &pids, const RssReader &reader, unsigned long long &out);
bool sumRssKb(const QList<int> &pids, unsigned long long &out);

} // namespace linuxproc
