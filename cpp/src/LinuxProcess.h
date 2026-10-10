#pragma once

#include "ProcessIdentity.h"

#include <QString>

namespace linuxproc {

bool exists(int pid);
QString comm(int pid);
unsigned long long startTime(int pid);
ProcessIdentity identity(int pid);

} // namespace linuxproc
