#pragma once

#include "ProcessIdentity.h"

#include <QString>

namespace linuxproc {

bool exists(int pid);
QString comm(int pid);
unsigned long long startTime(int pid);
ProcessIdentity identity(int pid);

bool cpuTicks(int pid, unsigned long long &out);
bool totalCpuTicks(unsigned long long &out);
bool rssKb(int pid, unsigned long long &out);

} // namespace linuxproc
