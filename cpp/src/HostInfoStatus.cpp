#include "HostInfoStatus.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

QString firstValue(const QByteArray &data, const QString &key) {
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromLatin1(raw);
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        if (line.left(colon).trimmed() == key) {
            return line.mid(colon + 1).trimmed();
        }
    }
    return QString();
}

bool isPseudoFs(const QString &fs) {
    static const QSet<QString> pseudo = {
        QStringLiteral("proc"),      QStringLiteral("sysfs"),
        QStringLiteral("cgroup"),    QStringLiteral("cgroup2"),
        QStringLiteral("devpts"),    QStringLiteral("mqueue"),
        QStringLiteral("debugfs"),   QStringLiteral("securityfs"),
        QStringLiteral("pstore"),    QStringLiteral("bpf"),
        QStringLiteral("tracefs"),   QStringLiteral("configfs"),
        QStringLiteral("fusectl"),   QStringLiteral("autofs"),
        QStringLiteral("hugetlbfs"), QStringLiteral("rpc_pipefs"),
        QStringLiteral("nsfs"),      QStringLiteral("devtmpfs"),
        QStringLiteral("binfmt_misc")};
    return pseudo.contains(fs);
}

} // namespace

HostCpuInfo parseCpuInfo(const QByteArray &cpuinfo) {
    HostCpuInfo info;
    const QList<QByteArray> lines = cpuinfo.split('\n');
    int logical = 0;
    int coresPerPackage = 0;
    double mhzSum = 0.0;
    int mhzCount = 0;
    QSet<QString> packageIds;
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromLatin1(raw);
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();
        if (key == QStringLiteral("processor")) {
            ++logical;
        } else if (key == QStringLiteral("vendor_id")) {
            info.vendorId = value;
        } else if (key == QStringLiteral("model name")) {
            info.modelName = value;
        } else if (key == QStringLiteral("physical id")) {
            packageIds.insert(value);
        } else if (key == QStringLiteral("cpu cores")) {
            coresPerPackage = value.toInt();
        } else if (key == QStringLiteral("cpu MHz")) {
            mhzSum += value.toDouble();
            ++mhzCount;
        } else if (key == QStringLiteral("cache size")) {
            info.cacheSize = value;
        }
    }
    info.logicalProcessors = logical;
    info.packages = packageIds.isEmpty() ? 1 : packageIds.size();
    info.cores = coresPerPackage > 0 ? coresPerPackage * info.packages
                                     : info.logicalProcessors;
    info.mhz = mhzCount > 0 ? mhzSum / mhzCount : 0.0;
    return info;
}

QList<HostFsInfo> parseMounts(const QByteArray &mounts) {
    QList<HostFsInfo> out;
    const QList<QByteArray> lines = mounts.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromLatin1(raw).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList parts =
            line.split(QRegularExpression(QStringLiteral("\\s+")),
                       Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            continue;
        }
        if (isPseudoFs(parts.at(2))) {
            continue;
        }
        HostFsInfo fs;
        fs.name = parts.at(0);
        fs.mountedOn = parts.at(1);
        out.append(fs);
    }
    return out;
}

QString parseOsRelease(const QByteArray &osRelease) {
    QString name;
    const QList<QByteArray> lines = osRelease.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromLatin1(raw).trimmed();
        if (line.startsWith(QStringLiteral("PRETTY_NAME="))) {
            QString value = line.mid(12);
            if (value.startsWith(QLatin1Char('"'))
                && value.endsWith(QLatin1Char('"'))) {
                value = value.mid(1, value.size() - 2);
            }
            return value;
        }
        if (line.startsWith(QStringLiteral("NAME="))) {
            name = line.mid(5);
        }
    }
    if (name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"'))) {
        name = name.mid(1, name.size() - 2);
    }
    return name;
}

HostInfo collectHostInfo() {
    HostInfo info;
    info.available = true;

    struct utsname uts {};
    if (uname(&uts) == 0) {
        info.platform.kernelName = QString::fromLatin1(uts.sysname);
        info.platform.kernelRelease = QString::fromLatin1(uts.release);
        info.platform.kernelVersion = QString::fromLatin1(uts.version);
        info.platform.machine = QString::fromLatin1(uts.machine);
    }
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        info.platform.hostname = QString::fromLatin1(hostname);
    }
    QFile osRelease(QStringLiteral("/etc/os-release"));
    if (osRelease.open(QIODevice::ReadOnly)) {
        info.platform.os = parseOsRelease(osRelease.readAll());
    }

    QFile cpuinfo(QStringLiteral("/proc/cpuinfo"));
    if (cpuinfo.open(QIODevice::ReadOnly)) {
        info.cpu = parseCpuInfo(cpuinfo.readAll());
    }
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly)) {
        const QByteArray data = meminfo.readAll();
        info.memTotalKb = firstValue(data, QStringLiteral("MemTotal"))
                              .split(QLatin1Char(' '))
                              .value(0)
                              .toULongLong();
        info.swapTotalKb = firstValue(data, QStringLiteral("SwapTotal"))
                               .split(QLatin1Char(' '))
                               .value(0)
                               .toULongLong();
    }

    QFile mounts(QStringLiteral("/proc/mounts"));
    if (mounts.open(QIODevice::ReadOnly)) {
        info.filesystems = parseMounts(mounts.readAll());
        for (HostFsInfo &fs : info.filesystems) {
            struct statvfs stats {};
            if (statvfs(fs.mountedOn.toUtf8().constData(), &stats) == 0) {
                fs.kbSize = static_cast<unsigned long long>(stats.f_blocks)
                            * stats.f_frsize / 1024ULL;
            }
        }
    }

    QHash<QString, HostNetworkInfo> byName;
    struct ifaddrs *addrs = nullptr;
    if (getifaddrs(&addrs) == 0) {
        for (struct ifaddrs *ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) {
                continue;
            }
            const QString name = QString::fromLatin1(ifa->ifa_name);
            if (name == QStringLiteral("lo")) {
                continue;
            }
            HostNetworkInfo &entry = byName[name];
            entry.name = name;
            if (entry.macAddress.isEmpty()) {
                QFile mac(QStringLiteral("/sys/class/net/") + name
                          + QStringLiteral("/address"));
                if (mac.open(QIODevice::ReadOnly)) {
                    entry.macAddress =
                        QString::fromLatin1(mac.readAll()).trimmed();
                }
            }
            const int family = ifa->ifa_addr->sa_family;
            auto prefixLength = [](const void *maskPtr, int bytes) {
                const auto *mask =
                    static_cast<const unsigned char *>(maskPtr);
                int bits = 0;
                for (int i = 0; i < bytes; ++i) {
                    unsigned char b = mask[i];
                    while ((b & 0x80) != 0) {
                        ++bits;
                        b = static_cast<unsigned char>(b << 1);
                    }
                }
                return bits;
            };
            if (family == AF_INET) {
                const auto *sin =
                    reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                char buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
                int prefix = 32;
                if (ifa->ifa_netmask != nullptr) {
                    prefix = prefixLength(
                        &reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask)
                             ->sin_addr,
                        4);
                }
                entry.ipv4 = QString::fromLatin1(buf)
                             + QStringLiteral("/%1").arg(prefix);
            } else if (family == AF_INET6) {
                const auto *sin6 =
                    reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
                char buf[INET6_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
                const QString ip = QString::fromLatin1(buf);
                if (!ip.startsWith(QStringLiteral("fe80"))) {
                    int prefix = 128;
                    if (ifa->ifa_netmask != nullptr) {
                        prefix = prefixLength(
                            &reinterpret_cast<struct sockaddr_in6 *>(
                                 ifa->ifa_netmask)
                                 ->sin6_addr,
                            16);
                    }
                    entry.ipv6 = ip + QStringLiteral("/%1").arg(prefix);
                }
            }
        }
        freeifaddrs(addrs);
    }
    for (auto it = byName.constBegin(); it != byName.constEnd(); ++it) {
        info.interfaces.append(it.value());
    }

    if (QFile::exists(QStringLiteral("/.dockerenv"))) {
        info.virtualization = QStringLiteral("docker");
    } else {
        QFile cgroup(QStringLiteral("/proc/1/cgroup"));
        QString cg;
        if (cgroup.open(QIODevice::ReadOnly)) {
            cg = QString::fromLatin1(cgroup.readAll());
        }
        QFile product(QStringLiteral("/sys/class/dmi/id/product_name"));
        QString prod;
        if (product.open(QIODevice::ReadOnly)) {
            prod = QString::fromLatin1(product.readAll()).trimmed();
        }
        if (cg.contains(QStringLiteral("lxc"))) {
            info.virtualization = QStringLiteral("lxc");
        } else if (prod.contains(QStringLiteral("KVM"))
                   || prod.contains(QStringLiteral("QEMU"))) {
            info.virtualization = QStringLiteral("kvm");
        } else if (prod.contains(QStringLiteral("VMware"))) {
            info.virtualization = QStringLiteral("vmware");
        } else if (prod.contains(QStringLiteral("VirtualBox"))) {
            info.virtualization = QStringLiteral("virtualbox");
        } else if (prod.contains(QStringLiteral("Xen"))) {
            info.virtualization = QStringLiteral("xen");
        } else {
            info.virtualization = QStringLiteral("none");
        }
    }
    return info;
}

QString hostInfoSummaryText(const HostInfo &info) {
    if (!info.available) {
        return QStringLiteral("host: unavailable");
    }
    return QStringLiteral("%1  %2 %3  %4 cores / %5 GB  %6")
        .arg(info.platform.hostname,
             info.platform.kernelName,
             info.platform.kernelRelease)
        .arg(info.cpu.logicalProcessors)
        .arg(QString::number(info.memTotalKb / (1024.0 * 1024.0), 'f', 1),
             info.virtualization);
}

QString hostInfoPlatformText(const HostInfo &info) {
    return QStringLiteral("os       %1\nhost     %2\nkernel   %3 %4\narch     %5")
        .arg(info.platform.os.isEmpty() ? QStringLiteral("-") : info.platform.os,
             info.platform.hostname, info.platform.kernelRelease,
             info.platform.kernelVersion.left(40), info.platform.machine);
}

QString hostInfoCpuText(const HostInfo &info) {
    return QStringLiteral("model    %1\nvendor   %2\ncores    %3 physical / %4 logical / %5 pkg\nmhz      %6\ncache    %7")
        .arg(info.cpu.modelName.isEmpty() ? QStringLiteral("-") : info.cpu.modelName,
             info.cpu.vendorId.isEmpty() ? QStringLiteral("-") : info.cpu.vendorId)
        .arg(info.cpu.cores)
        .arg(info.cpu.logicalProcessors)
        .arg(info.cpu.packages)
        .arg(QString::number(info.cpu.mhz, 'f', 0),
             info.cpu.cacheSize.isEmpty() ? QStringLiteral("-") : info.cpu.cacheSize);
}

QString hostInfoFilesystemsText(const HostInfo &info) {
    if (info.filesystems.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    int shown = 0;
    for (const HostFsInfo &fs : info.filesystems) {
        if (shown++ >= 12) {
            text += QStringLiteral("...\n");
            break;
        }
        text += QStringLiteral("%1  %2  %3 GB\n")
                    .arg(fs.mountedOn.left(28).leftJustified(28, ' '),
                         fs.name.left(20).leftJustified(20, ' '),
                         QString::number(fs.kbSize / (1024.0 * 1024.0), 'f', 1));
    }
    return text.trimmed();
}

QString hostInfoNetworkText(const HostInfo &info) {
    if (info.interfaces.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    for (const HostNetworkInfo &iface : info.interfaces) {
        text += QStringLiteral("%1  %2  %3%4\n")
                    .arg(iface.name.leftJustified(10, ' '),
                         iface.macAddress)
                    .arg(iface.ipv4.isEmpty() ? QStringLiteral("-") : iface.ipv4,
                         iface.ipv6.isEmpty()
                             ? QString()
                             : QStringLiteral("  ") + iface.ipv6);
    }
    return text.trimmed();
}
