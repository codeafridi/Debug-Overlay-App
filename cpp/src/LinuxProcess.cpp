#include "LinuxProcess.h"

#include "HostCommand.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <utility>

#include <cstdio>

namespace linuxproc {

namespace {

QString procPath(int pid, const char *leaf) {
    return QStringLiteral("/proc/%1/%2").arg(pid).arg(QLatin1String(leaf));
}

bool readStatTail(int pid, QStringList &fields) {
    QFile file(procPath(pid, "stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QString text = QString::fromUtf8(file.readAll());
    const int close = text.lastIndexOf(QLatin1Char(')'));
    if (close < 0 || close + 2 > text.size()) {
        return false;
    }

    fields = text.mid(close + 2).trimmed().split(QLatin1Char(' '),
                                                 Qt::SkipEmptyParts);
    return true;
}

// Reads every process's stat/status/comm/exe and the aggregate CPU line on the
// host in one shot. Output format:
//   cpu <...>              (first line, /proc/stat)
//   P\t<pid>\t<ppid>\t<utime>\t<stime>\t<start>\t<rss>\t<comm>\t<exe>
const char *kHostDumpScript = R"SH(
cat /proc/stat 2>/dev/null | head -n 1
for d in /proc/[0-9]*; do
  [ -r "$d/stat" ] || continue
  pid=${d#/proc/}
  stat=$(cat "$d/stat" 2>/dev/null) || continue
  rest=${stat##*) }
  set -- $rest
  ppid=${2:-}
  [ -n "$ppid" ] || continue
  utime=${12:-0}
  stime=${13:-0}
  start=${20:-0}
  rss=0
  if [ -r "$d/status" ]; then
    while read -r k v _; do
      if [ "$k" = "VmRSS:" ]; then rss=$v; break; fi
    done < "$d/status"
  fi
  comm=$(cat "$d/comm" 2>/dev/null | tr -d '\n\r')
  exe=$(readlink "$d/exe" 2>/dev/null)
  printf 'P\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$pid" "$ppid" "$utime" "$stime" "$start" "$rss" "$comm" "$exe"
done
)SH";

struct HostSnapshot {
    bool valid = false;
    bool attempted = false;
    qint64 attemptedAtMs = 0;
    QHash<int, ProcEntry> entries;
    unsigned long long totalTicks = 0;
};

HostSnapshot &snapshot() {
    static HostSnapshot value;
    return value;
}

bool fetchHostSnapshot() {
    const QString program =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("SYSPEEK_HOST_SPAWN"),
            QStringLiteral("flatpak-spawn"));

    QProcess process;
    process.start(program,
                  {QStringLiteral("--host"), QStringLiteral("sh"),
                   QStringLiteral("-c"), QString::fromUtf8(kHostDumpScript)});
    if (!process.waitForStarted(500)) {
        return false;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(100);
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        return false;
    }

    unsigned long long total = 0;
    QHash<int, ProcEntry> entries =
        parseHostProcDump(process.readAllStandardOutput(), total);

    HostSnapshot fresh;
    fresh.valid = true;
    fresh.attempted = true;
    fresh.attemptedAtMs = QDateTime::currentMSecsSinceEpoch();
    fresh.entries = std::move(entries);
    fresh.totalTicks = total;
    snapshot() = fresh;
    return true;
}

bool ensureSnapshot() {
    HostSnapshot &s = snapshot();
    if (s.valid) {
        return true;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (s.attempted && (now - s.attemptedAtMs) < 500) {
        return false;
    }
    s.attempted = true;
    s.attemptedAtMs = now;
    return fetchHostSnapshot();
}

const ProcEntry *hostEntry(int pid) {
    if (!ensureSnapshot()) {
        return nullptr;
    }
    const HostSnapshot &s = snapshot();
    auto it = s.entries.constFind(pid);
    return it == s.entries.constEnd() ? nullptr : &it.value();
}

} // namespace

bool hostProcEnabled() {
    return hostcmd::inFlatpak();
}

void beginSample() {
    HostSnapshot &s = snapshot();
    s.valid = false;
    s.attempted = false;
}

QHash<int, ProcEntry> parseHostProcDump(const QByteArray &output,
                                        unsigned long long &totalTicks) {
    QHash<int, ProcEntry> entries;
    totalTicks = 0;
    bool cpuSeen = false;

    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &raw : lines) {
        if (raw.isEmpty()) {
            continue;
        }

        if (!cpuSeen && raw.startsWith("cpu")) {
            const QList<QByteArray> parts = raw.simplified().split(' ');
            constexpr int countedFields = 8;
            unsigned long long sum = 0;
            int counted = 0;
            for (int i = 1; i < parts.size() && counted < countedFields;
                 ++i, ++counted) {
                bool ok = false;
                const unsigned long long value = parts.at(i).toULongLong(&ok);
                if (!ok) {
                    break;
                }
                sum += value;
            }
            totalTicks = sum;
            cpuSeen = true;
            continue;
        }

        if (!raw.startsWith("P\t")) {
            continue;
        }

        const QList<QByteArray> fields = raw.split('\t');
        if (fields.size() < 9) {
            continue;
        }

        ProcEntry entry;
        entry.pid = fields.at(1).toInt();
        entry.ppid = fields.at(2).toInt();
        entry.utime = fields.at(3).toULongLong();
        entry.stime = fields.at(4).toULongLong();
        entry.startTime = fields.at(5).toULongLong();
        entry.rssKb = fields.at(6).toULongLong();
        entry.comm = QString::fromUtf8(fields.at(7));
        entry.exe = QString::fromUtf8(fields.at(8));

        if (entry.pid > 0) {
            entries.insert(entry.pid, entry);
        }
    }

    return entries;
}

bool exists(int pid) {
    if (pid <= 0) {
        return false;
    }
    if (hostProcEnabled()) {
        return hostEntry(pid) != nullptr;
    }
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

QString comm(int pid) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        return entry != nullptr ? entry->comm : QString();
    }

    QFile file(procPath(pid, "comm"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString exePath(int pid) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        return entry != nullptr ? entry->exe : QString();
    }

    const QString path = procPath(pid, "exe");
    return QFile::symLinkTarget(path);
}

int parentPid(int pid) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        return entry != nullptr ? entry->ppid : -1;
    }

    QStringList fields;
    if (!readStatTail(pid, fields) || fields.size() <= 1) {
        return -1;
    }

    bool ok = false;
    const int ppid = fields.at(1).toInt(&ok);
    return ok ? ppid : -1;
}

QList<int> allPids() {
    if (hostProcEnabled()) {
        if (!ensureSnapshot()) {
            return {};
        }
        return snapshot().entries.keys();
    }

    QList<int> pids;
    const QStringList entries =
        QDir(QStringLiteral("/proc"))
            .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (ok) {
            pids.append(pid);
        }
    }
    return pids;
}

unsigned long long startTime(int pid) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        return entry != nullptr ? entry->startTime : 0;
    }

    QStringList fields;
    if (!readStatTail(pid, fields) || fields.size() <= 19) {
        return 0;
    }

    bool ok = false;
    const unsigned long long value = fields.at(19).toULongLong(&ok);
    return ok ? value : 0;
}

ProcessIdentity identity(int pid) {
    ProcessIdentity id;
    if (!exists(pid)) {
        return id;
    }

    id.pid = pid;
    id.startTime = startTime(pid);
    if (id.startTime == 0) {
        id.pid = -1;
    }
    return id;
}

bool cpuTicks(int pid, unsigned long long &out) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        if (entry == nullptr) {
            return false;
        }
        out = entry->utime + entry->stime;
        return true;
    }

    QStringList fields;
    if (!readStatTail(pid, fields) || fields.size() <= 12) {
        return false;
    }

    bool okU = false;
    bool okS = false;
    const unsigned long long utime = fields.at(11).toULongLong(&okU);
    const unsigned long long stime = fields.at(12).toULongLong(&okS);
    if (!okU || !okS) {
        return false;
    }

    out = utime + stime;
    return true;
}

bool totalCpuTicks(unsigned long long &out) {
    if (hostProcEnabled()) {
        if (!ensureSnapshot()) {
            return false;
        }
        out = snapshot().totalTicks;
        return true;
    }

    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray line = file.readLine().simplified();
    const QList<QByteArray> parts = line.split(' ');
    if (parts.isEmpty() || parts.first() != QByteArrayLiteral("cpu")) {
        return false;
    }

    constexpr int countedFields = 8;
    unsigned long long sum = 0;
    int counted = 0;
    for (int i = 1; i < parts.size() && counted < countedFields; ++i, ++counted) {
        bool ok = false;
        const unsigned long long value = parts.at(i).toULongLong(&ok);
        if (!ok) {
            break;
        }
        sum += value;
    }

    out = sum;
    return true;
}

bool rssKb(int pid, unsigned long long &out) {
    if (hostProcEnabled()) {
        const ProcEntry *entry = hostEntry(pid);
        if (entry == nullptr) {
            return false;
        }
        out = entry->rssKb;
        return true;
    }

    QFile file(procPath(pid, "status"));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
        if (!line.startsWith("VmRSS:")) {
            continue;
        }

        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 2) {
            return false;
        }

        bool ok = false;
        const unsigned long long kb = parts.at(1).toULongLong(&ok);
        if (!ok) {
            return false;
        }

        out = kb;
        return true;
    }

    return false;
}

bool sumRssKb(const QList<int> &pids, unsigned long long &out) {
    unsigned long long total = 0;
    bool any = false;

    for (int pid : pids) {
        unsigned long long kb = 0;
        if (rssKb(pid, kb)) {
            total += kb;
            any = true;
        }
    }

    if (!any) {
        return false;
    }

    out = total;
    return true;
}

} // namespace linuxproc
