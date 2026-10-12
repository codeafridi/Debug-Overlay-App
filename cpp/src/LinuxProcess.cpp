#include "LinuxProcess.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <unistd.h>

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

} // namespace

bool exists(int pid) {
    if (pid <= 0) {
        return false;
    }
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

QString comm(int pid) {
    QFile file(procPath(pid, "comm"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString exePath(int pid) {
    const QString path = procPath(pid, "exe");
    return QFile::symLinkTarget(path);
}

int parentPid(int pid) {
    QStringList fields;
    if (!readStatTail(pid, fields) || fields.size() <= 1) {
        return -1;
    }

    bool ok = false;
    const int ppid = fields.at(1).toInt(&ok);
    return ok ? ppid : -1;
}

QList<int> allPids() {
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

bool procCpu(int pid, unsigned long long &ticks, unsigned long long &startTime) {
    QStringList fields;
    if (!readStatTail(pid, fields) || fields.size() <= 19) {
        return false;
    }

    bool okU = false;
    bool okS = false;
    bool okT = false;
    const unsigned long long utime = fields.at(11).toULongLong(&okU);
    const unsigned long long stime = fields.at(12).toULongLong(&okS);
    const unsigned long long start = fields.at(19).toULongLong(&okT);
    if (!okU || !okS || !okT || start == 0) {
        return false;
    }

    ticks = utime + stime;
    startTime = start;
    return true;
}

int onlineCpuCount() {
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    // procfs files report size 0, so atEnd()/readLine() loops are unreliable;
    // read the whole pseudo-file once and split it.
    const QByteArray data = file.readAll();
    int count = 0;
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &line : lines) {
        if (line.size() >= 4 && line.startsWith(QByteArrayLiteral("cpu"))
            && line.at(3) >= '0' && line.at(3) <= '9') {
            ++count;
        }
    }
    return count;
}

bool totalCpuTicks(unsigned long long &out) {
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
    QFile file(procPath(pid, "status"));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return parseVmRssKb(file.readAll(), out);
}

bool parseVmRssKb(const QByteArray &statusContent, unsigned long long &out) {
    const QList<QByteArray> lines = statusContent.split('\n');
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

namespace {

unsigned long long parseKbValue(const QByteArray &line, bool &ok) {
    const QList<QByteArray> parts = line.simplified().split(' ');
    if (parts.size() < 2) {
        ok = false;
        return 0;
    }
    return parts.at(1).toULongLong(&ok);
}

} // namespace

bool parseStatusMem(const QByteArray &statusContent, MemInfo &out) {
    bool foundRss = false;

    const QList<QByteArray> lines = statusContent.split('\n');
    for (const QByteArray &line : lines) {
        bool ok = false;
        if (line.startsWith("VmRSS:")) {
            const unsigned long long kb = parseKbValue(line, ok);
            if (ok) {
                out.rssKb = kb;
                foundRss = true;
            }
        } else if (line.startsWith("VmSize:")) {
            const unsigned long long kb = parseKbValue(line, ok);
            if (ok) {
                out.vmsKb = kb;
            }
        } else if (line.startsWith("VmSwap:")) {
            const unsigned long long kb = parseKbValue(line, ok);
            if (ok) {
                out.swapKb = kb;
            }
        }
    }

    return foundRss;
}

bool parseStatusDetail(const QByteArray &statusContent, ProcDetail &out) {
    const QList<QByteArray> lines = statusContent.split('\n');
    for (const QByteArray &line : lines) {
        bool ok = false;
        if (line.startsWith("Threads:")) {
            const unsigned long long value = parseKbValue(line, ok);
            if (ok) {
                out.threads = static_cast<int>(value);
            }
        } else if (line.startsWith("voluntary_ctxt_switches:")) {
            const unsigned long long value = parseKbValue(line, ok);
            if (ok) {
                out.voluntaryCtx = value;
            }
        } else if (line.startsWith("nonvoluntary_ctxt_switches:")) {
            const unsigned long long value = parseKbValue(line, ok);
            if (ok) {
                out.involuntaryCtx = value;
            }
        }
    }
    return true;
}

int countFds(int pid) {
    const QString path = procPath(pid, "fd");
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir() || !info.isReadable()) {
        return -1;
    }
    const QStringList entries = QDir(path).entryList(QDir::NoDotAndDotDot);
    return entries.size();
}

namespace {

QString socketStateName(int code) {
    switch (code) {
    case 0x01:
        return QStringLiteral("ESTABLISHED");
    case 0x02:
        return QStringLiteral("SYN_SENT");
    case 0x03:
        return QStringLiteral("SYN_RECV");
    case 0x04:
        return QStringLiteral("FIN_WAIT1");
    case 0x05:
        return QStringLiteral("FIN_WAIT2");
    case 0x06:
        return QStringLiteral("TIME_WAIT");
    case 0x07:
        return QStringLiteral("CLOSE");
    case 0x08:
        return QStringLiteral("CLOSE_WAIT");
    case 0x09:
        return QStringLiteral("LAST_ACK");
    case 0x0A:
        return QStringLiteral("LISTEN");
    case 0x0B:
        return QStringLiteral("CLOSING");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

QString ipv4FromHex(const QString &hex) {
    if (hex.size() != 8) {
        return hex;
    }
    bool ok = false;
    const quint32 value = hex.toUInt(&ok, 16);
    if (!ok) {
        return hex;
    }
    return QStringLiteral("%1.%2.%3.%4")
        .arg(value & 0xFF)
        .arg((value >> 8) & 0xFF)
        .arg((value >> 16) & 0xFF)
        .arg((value >> 24) & 0xFF);
}

QString ipv6FromHex(const QString &hex) {
    if (hex.size() != 32) {
        return hex;
    }

    quint16 groups[8];
    for (int w = 0; w < 4; ++w) {
        bool ok = false;
        const quint32 word = hex.mid(w * 8, 8).toUInt(&ok, 16);
        if (!ok) {
            return hex;
        }
        groups[w * 2] = static_cast<quint16>(((word & 0xFF) << 8)
                                             | ((word >> 8) & 0xFF));
        groups[w * 2 + 1] = static_cast<quint16>(((word >> 16) & 0xFF) << 8
                                                 | ((word >> 24) & 0xFF));
    }

    int bestStart = -1;
    int bestLen = 0;
    for (int i = 0; i < 8;) {
        if (groups[i] != 0) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 8 && groups[j] == 0) {
            ++j;
        }
        if (j - i > bestLen) {
            bestLen = j - i;
            bestStart = i;
        }
        i = j;
    }

    QString out;
    for (int i = 0; i < 8; ++i) {
        if (i == bestStart && bestLen > 1) {
            out += QStringLiteral("::");
            i += bestLen - 1;
            continue;
        }
        if (!out.isEmpty() && !out.endsWith(QLatin1Char(':'))) {
            out += QLatin1Char(':');
        }
        out += QString::number(groups[i], 16);
    }
    return out.isEmpty() ? QStringLiteral("::") : out;
}

} // namespace

QList<unsigned long long> socketInodes(int pid) {
    QList<unsigned long long> inodes;
    const QString dir = procPath(pid, "fd");
    const QStringList entries =
        QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        // QFile::symLinkTarget() rewrites a relative target against the link's
        // directory ("socket:[123]" -> "/proc/<pid>/fd/socket:[123]"), so it
        // never matches on procfs. readlink() returns the raw target.
        const QByteArray path = (dir + QLatin1Char('/') + entry).toLocal8Bit();
        char buffer[128];
        const ssize_t length =
            ::readlink(path.constData(), buffer, sizeof(buffer) - 1);
        if (length <= 0) {
            continue;
        }
        const QByteArray target(buffer, static_cast<int>(length));
        if (!target.startsWith("socket:[") || !target.endsWith(']')) {
            continue;
        }
        bool ok = false;
        const unsigned long long inode =
            target.mid(8, target.size() - 9).toULongLong(&ok);
        if (ok) {
            inodes.append(inode);
        }
    }
    return inodes;
}

bool parseNetTable(const QByteArray &content, const QString &proto,
                   QList<SocketInfo> &out) {
    const QList<QByteArray> lines = content.split('\n');
    if (lines.isEmpty()) {
        return false;
    }

    const bool ipv6 = proto.endsWith(QLatin1Char('6'));
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = lines.at(i).simplified().split(' ');
        if (fields.size() < 10) {
            continue;
        }

        const QString local = QString::fromLatin1(fields.at(1));
        const QString remote = QString::fromLatin1(fields.at(2));
        const int localColon = local.indexOf(QLatin1Char(':'));
        const int remoteColon = remote.indexOf(QLatin1Char(':'));
        if (localColon < 0 || remoteColon < 0) {
            continue;
        }

        SocketInfo info;
        info.proto = proto;
        info.localAddr = ipv6 ? ipv6FromHex(local.left(localColon))
                              : ipv4FromHex(local.left(localColon));
        info.localPort = local.mid(localColon + 1).toUShort(nullptr, 16);
        info.remoteAddr = ipv6 ? ipv6FromHex(remote.left(remoteColon))
                               : ipv4FromHex(remote.left(remoteColon));
        info.remotePort = remote.mid(remoteColon + 1).toUShort(nullptr, 16);

        bool ok = false;
        const int state = fields.at(3).toInt(&ok, 16);
        info.state = ok ? socketStateName(state) : QStringLiteral("UNKNOWN");
        info.inode = fields.at(9).toULongLong();
        out.append(info);
    }
    return true;
}

QList<SocketInfo> readNetSockets() {
    QList<SocketInfo> all;
    const char *const files[] = {"tcp", "tcp6", "udp", "udp6"};
    for (const char *file : files) {
        QFile handle(QStringLiteral("/proc/net/%1").arg(QLatin1String(file)));
        if (!handle.open(QIODevice::ReadOnly)) {
            continue;
        }
        QList<SocketInfo> part;
        parseNetTable(handle.readAll(), QString::fromLatin1(file), part);
        all += part;
    }
    return all;
}

bool parseStatmShared(const QByteArray &statmContent, unsigned long long pageSize,
                      unsigned long long &sharedKb) {
    const QList<QByteArray> parts = statmContent.simplified().split(' ');
    if (parts.size() < 3) {
        return false;
    }

    bool ok = false;
    const unsigned long long pages = parts.at(2).toULongLong(&ok);
    if (!ok) {
        return false;
    }

    sharedKb = pages * pageSize / 1024;
    return true;
}

bool procMem(int pid, MemInfo &out) {
    QFile status(procPath(pid, "status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!parseStatusMem(status.readAll(), out)) {
        return false;
    }

    QFile statm(procPath(pid, "statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const long page = ::sysconf(_SC_PAGESIZE);
        const unsigned long long pageSize =
            page > 0 ? static_cast<unsigned long long>(page) : 4096ULL;
        unsigned long long shared = 0;
        if (parseStatmShared(statm.readAll(), pageSize, shared)) {
            out.sharedKb = shared;
        }
    }

    return true;
}

bool procSample(int pid, MemInfo &mem, ProcDetail &detail) {
    QFile status(procPath(pid, "status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray content = status.readAll();
    if (!parseStatusMem(content, mem)) {
        return false;
    }
    parseStatusDetail(content, detail);

    QFile statm(procPath(pid, "statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const long page = ::sysconf(_SC_PAGESIZE);
        const unsigned long long pageSize =
            page > 0 ? static_cast<unsigned long long>(page) : 4096ULL;
        unsigned long long shared = 0;
        if (parseStatmShared(statm.readAll(), pageSize, shared)) {
            mem.sharedKb = shared;
        }
    }

    detail.fdCount = countFds(pid);
    return true;
}

bool sumRssKbWith(const QList<int> &pids, const RssReader &reader, unsigned long long &out) {
    unsigned long long total = 0;
    bool any = false;

    for (int pid : pids) {
        unsigned long long kb = 0;
        if (reader && reader(pid, kb)) {
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

bool sumRssKb(const QList<int> &pids, unsigned long long &out) {
    return sumRssKbWith(
        pids, [](int pid, unsigned long long &kb) { return rssKb(pid, kb); }, out);
}

} // namespace linuxproc
