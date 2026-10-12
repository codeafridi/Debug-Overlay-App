#pragma once

#include "ProcessIdentity.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>

namespace linuxproc {

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
using RssReader = std::function<bool(int, unsigned long long &)>;
bool sumRssKbWith(const QList<int> &pids, const RssReader &reader, unsigned long long &out);
bool sumRssKb(const QList<int> &pids, unsigned long long &out);

} // namespace linuxproc
