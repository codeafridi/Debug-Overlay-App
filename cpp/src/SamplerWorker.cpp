#include "SamplerWorker.h"

#include "FocusedWindowProvider.h"
#include "LinuxProcess.h"
#include "MemoryFormat.h"
#include "ProcessGrouper.h"

#include <QSet>
#include <QTimer>

#include <algorithm>

SamplerWorker::SamplerWorker(FocusedWindowProvider *provider,
                             const Config &config,
                             QObject *parent)
    : QObject(parent)
    , m_provider(provider)
    , m_sampler(provider)
    , m_health(config.health) {
    m_intervalMs = config.sampleIntervalMs;
}

void SamplerWorker::start() {
    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setInterval(m_intervalMs);
        connect(m_timer, &QTimer::timeout, this, &SamplerWorker::sampleNow);
    }
    m_timer->start();
    sampleNow();
}

void SamplerWorker::stop() {
    if (m_timer != nullptr) {
        m_timer->stop();
    }
}

void SamplerWorker::sampleNow() {
    const FocusSample focus = m_sampler.sample();
    DisplaySnapshot snapshot = buildSnapshot(focus);
    snapshot.sequence = ++m_sequence;
    emit snapshotReady(snapshot);
}

DisplaySnapshot SamplerWorker::buildSnapshot(const FocusSample &focus) {
    DisplaySnapshot snap;

    if (!focus.available) {
        snap.status = QStringLiteral("no-focus");
        m_cpuText = QStringLiteral("--");
        m_memText = QStringLiteral("--");
        m_cpuIdentity = ProcessIdentity{};
        m_healthIdentity = ProcessIdentity{};
        m_health.reset();
        return snap;
    }

    if (focus.identity != m_healthIdentity) {
        m_health.reset();
        m_healthIdentity = focus.identity;
    }

    snap.appName = focus.appName;
    snap.pid = QString::number(focus.identity.pid);

    const ProcessGroup &group = m_groupCache.pidsFor(focus.identity);
    const CpuSample cpu = m_cpuSampler.sample(group.pids, focus.identity);
    const bool cpuValid = cpu.available && !cpu.warming;

    if (!cpu.available) {
        m_cpuText = QStringLiteral("--");
    } else if (cpu.warming) {
        m_cpuText = QStringLiteral("Warming up");
    } else {
        m_cpuText = QStringLiteral("%1%").arg(qRound(cpu.percent));
    }

    unsigned long long rss = 0;
    const bool rssValid = linuxproc::sumRssKb(group.pids, rss);
    m_memText = rssValid ? formatMemoryKb(rss) : QStringLiteral("--");

    QHash<int, int> parentOf;
    for (int pid : group.pids) {
        const int ppid = linuxproc::parentPid(pid);
        if (ppid > 0) {
            parentOf.insert(pid, ppid);
        }
    }

    const QList<ProcNode> tree =
        ProcessGrouper::orderedTree(group.rootPid, parentOf);

    unsigned long long rssTotal = 0;
    unsigned long long vmsTotal = 0;
    unsigned long long swapTotal = 0;
    unsigned long long sharedTotal = 0;
    QList<ProcessRow> rows;
    linuxproc::ProcDetail rootDetail;
    int rootPid = -1;
    for (const ProcNode &node : tree) {
        linuxproc::MemInfo info;
        linuxproc::ProcDetail detail;
        if (!linuxproc::procSample(node.pid, info, detail)) {
            continue;
        }
        rssTotal += info.rssKb;
        vmsTotal += info.vmsKb;
        swapTotal += info.swapKb;
        sharedTotal += info.sharedKb;

        if (node.depth == 0) {
            rootPid = node.pid;
            rootDetail = detail;
        }

        ProcessRow row;
        row.pid = node.pid;
        row.depth = node.depth;
        row.threads = detail.threads;
        row.fds = detail.fdCount;
        row.cpuPercent = cpu.perPidPercent.value(node.pid, 0.0);
        row.name = linuxproc::comm(node.pid);
        if (row.name.isEmpty()) {
            row.name = QString::number(node.pid);
        }
        row.memory = formatMemoryKb(info.rssKb);
        rows.append(row);
    }

    if (rootPid > 0) {
        const IoRateTracker::Rate rate =
            m_ioTracker.update(rootPid, rootDetail.rchar, rootDetail.wchar);
        snap.focusThreads = rootDetail.threads;
        snap.focusFds = rootDetail.fdCount;
        snap.focusVolCtx = rootDetail.voluntaryCtx;
        snap.focusInvolCtx = rootDetail.involuntaryCtx;
        snap.focusIoReadPerSec = rate.readPerSec;
        snap.focusIoWritePerSec = rate.writePerSec;
        snap.focusOpenFiles = linuxproc::fdTargets(rootPid);
        linuxproc::sharedLibraries(rootPid, snap.dependencies);
    }

    snap.processes = rows;
    snap.rssKb = rssTotal;
    snap.vmsKb = vmsTotal;
    snap.swapKb = swapTotal;
    snap.sharedKb = sharedTotal;

    QSet<unsigned long long> inodeSet;
    for (const ProcNode &node : tree) {
        const QList<unsigned long long> inodes = linuxproc::socketInodes(node.pid);
        for (unsigned long long inode : inodes) {
            inodeSet.insert(inode);
        }
    }

    QList<ConnectionRow> connections;
    QList<QString> listening;
    QList<EndpointRow> endpoints;
    QList<QString> dbConnections;
    QHash<QString, int> stateCounts;
    QSet<QString> listenSeen;
    const QList<linuxproc::SocketInfo> sockets = linuxproc::readNetSockets();
    for (const linuxproc::SocketInfo &socket : sockets) {
        stateCounts[socket.state] += 1;

        const bool isEndpoint =
            socket.state == QStringLiteral("LISTEN")
            || (socket.proto.startsWith(QStringLiteral("udp"))
                && socket.remotePort == 0);
        if (isEndpoint) {
            const QString endpoint = QStringLiteral("%1 %2:%3")
                                         .arg(socket.proto, socket.localAddr)
                                         .arg(socket.localPort);
            if (!listenSeen.contains(endpoint)) {
                listenSeen.insert(endpoint);
                listening.append(endpoint);

                EndpointRow entry;
                entry.proto = socket.proto;
                entry.addr = socket.localAddr;
                entry.port = socket.localPort;
                entry.exposure = linuxproc::endpointExposure(socket.localAddr);
                entry.service = linuxproc::serviceForPort(socket.localPort);
                entry.database = linuxproc::portIsDatabase(socket.localPort);
                endpoints.append(entry);
            }
        }

        if (!inodeSet.contains(socket.inode) || connections.size() >= 50) {
            continue;
        }
        ConnectionRow row;
        row.proto = socket.proto;
        row.local =
            QStringLiteral("%1:%2").arg(socket.localAddr).arg(socket.localPort);
        row.remote =
            (socket.remotePort != 0
             || socket.state == QStringLiteral("ESTABLISHED"))
                ? QStringLiteral("%1:%2")
                      .arg(socket.remoteAddr)
                      .arg(socket.remotePort)
                : QStringLiteral("*");
        row.state = socket.state;
        connections.append(row);

        if (linuxproc::portIsDatabase(socket.remotePort)) {
            dbConnections.append(
                QStringLiteral("%1:%2 -> %3:%4 (%5)")
                    .arg(socket.localAddr)
                    .arg(socket.localPort)
                    .arg(socket.remoteAddr)
                    .arg(socket.remotePort)
                    .arg(linuxproc::serviceForPort(socket.remotePort)));
        }
    }
    snap.connections = connections;
    snap.listeningPorts = listening;
    snap.endpoints = endpoints;
    snap.dbConnections = dbConnections;

    linuxproc::processInstrumentation(focus.identity.pid, snap.instrumentation);

    QList<QPair<QString, int>> counts;
    for (auto it = stateCounts.constBegin(); it != stateCounts.constEnd(); ++it) {
        counts.append(qMakePair(it.key(), it.value()));
    }
    std::sort(counts.begin(), counts.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.first < b.first;
              });
    snap.socketStateCounts = counts;

    QList<linuxproc::InterfaceStats> interfaceStats;
    if (linuxproc::hostInterfaces(interfaceStats)) {
        QList<InterfaceRow> interfaceRows;
        for (const linuxproc::InterfaceStats &stats : interfaceStats) {
            InterfaceRow row;
            row.name = stats.name;
            row.rxBytes = stats.rxBytes;
            row.txBytes = stats.txBytes;
            const NetRateTracker::Rate rate =
                m_netTracker.update(stats.name, stats.rxBytes, stats.txBytes);
            row.rxPerSec = rate.rxPerSec;
            row.txPerSec = rate.txPerSec;
            interfaceRows.append(row);
        }
        snap.interfaces = interfaceRows;
    }

    linuxproc::CgroupInfo cgroup;
    if (linuxproc::cgroupSample(focus.identity.pid, cgroup) && cgroup.available) {
        snap.cgroupAvailable = true;
        snap.cgroupVersion = cgroup.version;
        snap.cgroupPath = cgroup.path;
        snap.cgroupMemUsage = cgroup.memUsageBytes;
        snap.cgroupMemLimit = cgroup.memLimitBytes;
        snap.cgroupAnon = cgroup.memAnonBytes;
        snap.cgroupFile = cgroup.memFileBytes;
        snap.cgroupKernel = cgroup.memKernelBytes;
        snap.cgroupThrottled = cgroup.nrThrottled;
        snap.cgroupPeriods = cgroup.nrPeriods;
        snap.cgroupThrottledUsec = cgroup.throttledUsec;
    }

    m_cpuIdentity = focus.identity;

    snap.cpu = m_cpuText;
    snap.memory = m_memText;
    snap.cpuValid = cpuValid;
    snap.cpuPercent = cpuValid ? cpu.percent : 0.0;
    snap.status = HealthEvaluator::text(
        m_health.update(cpuValid, cpuValid ? cpu.percent : 0.0, rssValid, rss));

    const HostSample host = m_hostSampler.sample();
    if (host.available) {
        snap.hostAvailable = true;
        snap.hostWarming = host.warming;
        snap.hostCpuPercent = host.cpuPercent;
        snap.hostMemTotalKb = host.mem.totalKb;
        snap.hostMemAvailableKb = host.mem.availableKb;
        snap.hostDiskTotalBytes = host.diskTotalBytes;
        snap.hostDiskFreeBytes = host.diskFreeBytes;
        snap.hostNetRxPerSec = host.netRxPerSec;
        snap.hostNetTxPerSec = host.netTxPerSec;
        snap.hostLoad1 = host.load1;
    }

    return snap;
}
