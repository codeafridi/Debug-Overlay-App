#include "MetricStore.h"
#include "Sparkline.h"
#include "IoRateTracker.h"
#include "NetRateTracker.h"
#include "ServicesStatus.h"
#include "JournalStatus.h"
#include "AlertEngine.h"
#include "SyntheticStatus.h"
#include "DiskStatus.h"
#include "TcpStatus.h"
#include "SecurityStatus.h"
#include "LogParser.h"
#include "TraceStatus.h"

#include <QApplication>
#include <QPixmap>
#include <QThread>

#include <cmath>
#include <cstdio>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    MetricStore store(3);
    check(store.capacity() == 3, "store capacity");
    check(store.size() == 0, "store starts empty");
    store.push(10);
    store.push(20);
    check(store.size() == 2 && store.last() == 20, "store push/last");
    check(store.minimum() == 10 && store.maximum() == 20, "store min/max");
    store.push(30);
    store.push(40);
    check(store.size() == 3, "store trims to capacity");
    check(store.values().first() == 20 && store.values().last() == 40,
          "store keeps most recent window");
    store.clear();
    check(store.size() == 0 && store.last() == 0.0, "store clear");

    Sparkline spark;
    spark.resize(120, 24);
    check(spark.valueCount() == 0, "sparkline starts empty");
    check(spark.grab().width() == 120, "sparkline renders empty");
    spark.setValues({1.0, 2.0, 3.0, 2.0});
    check(spark.valueCount() == 4, "sparkline stores values");
    check(!spark.grab().isNull(), "sparkline renders a polyline");
    spark.setValues({5.0});
    spark.grab();
    check(true, "sparkline handles a single value");
    spark.setValues({});
    check(spark.valueCount() == 0, "sparkline accepts empty reset");

    IoRateTracker tracker;
    const IoRateTracker::Rate first = tracker.update(42, 1000, 2000);
    check(first.readPerSec == 0.0 && first.writePerSec == 0.0,
          "io tracker: first sample -> 0");
    QThread::msleep(30);
    const IoRateTracker::Rate second = tracker.update(42, 4000, 8000);
    check(second.readPerSec > 0.0 && second.writePerSec > 0.0,
          "io tracker: counters growing -> positive rate");
    const IoRateTracker::Rate reset = tracker.update(42, 100, 100);
    check(reset.readPerSec == 0.0 && reset.writePerSec == 0.0,
          "io tracker: counter reset -> 0");
    const IoRateTracker::Rate noPerm = tracker.update(7, -1, -1);
    check(noPerm.readPerSec == 0.0, "io tracker: no permission -> 0");

    NetRateTracker netTracker;
    const NetRateTracker::Rate n1 = netTracker.update(QStringLiteral("eth0"), 1000, 2000);
    check(n1.rxPerSec == 0.0 && n1.txPerSec == 0.0,
          "net tracker: first sample -> 0");
    QThread::msleep(30);
    const NetRateTracker::Rate n2 = netTracker.update(QStringLiteral("eth0"), 4000, 5000);
    check(n2.rxPerSec > 0.0 && n2.txPerSec > 0.0,
          "net tracker: counters growing -> positive rate");
    const NetRateTracker::Rate n3 = netTracker.update(QStringLiteral("eth0"), 100, 100);
    check(n3.rxPerSec == 0.0 && n3.txPerSec == 0.0,
          "net tracker: counter reset -> 0");

    QList<UnitRow> units;
    check(parseSystemctlUnits(
              "nginx.service loaded active running A high performance web server\n"
              "db.service loaded failed failed Database\n"
              "ssh.service loaded active exited OpenBSD Secure Shell server\n"
              "19 loaded units listed. Pass --all to see inactive units.\n",
              units)
              && units.size() == 3
              && units.at(0).name == QStringLiteral("nginx.service")
              && units.at(0).active == QStringLiteral("active")
              && units.at(0).sub == QStringLiteral("running")
              && units.at(0).description
                     == QStringLiteral("A high performance web server")
              && units.at(1).active == QStringLiteral("failed"),
          "parseSystemctlUnits: rows + description, footer skipped");
    check(servicesRunningCount(units) == 1, "servicesRunningCount == 1");
    check(servicesFailedCount(units) == 1, "servicesFailedCount == 1");

    QList<UnitRow> bulletUnits;
    check(parseSystemctlUnits(
              "\xE2\x97\x8F boom.service loaded failed failed Boom\n",
              bulletUnits)
              && bulletUnits.size() == 1
              && bulletUnits.at(0).name == QStringLiteral("boom.service")
              && bulletUnits.at(0).active == QStringLiteral("failed"),
          "parseSystemctlUnits: bullet-prefixed failed unit");

    ServicesSnapshot services;
    services.available = true;
    services.runningCount = 3;
    services.failedCount = 1;
    check(servicesSummaryText(services).contains(QStringLiteral("3 running"))
              && servicesSummaryText(services).contains(QStringLiteral("1 failed")),
          "services summary text");

    QList<JournalEntry> entries;
    const QByteArray journal =
        "{\"__REALTIME_TIMESTAMP\":\"1791907430214266\",\"PRIORITY\":\"3\","
        "\"_PID\":\"2196\",\"_COMM\":\"nginx\",\"MESSAGE\":\"boom\"}\n"
        "{\"PRIORITY\":\"6\",\"_COMM\":\"cron\",\"MESSAGE\":\"tick\"}\n"
        "not json\n"
        "{\"PRIORITY\":\"2\",\"_COMM\":\"kernel\",\"MESSAGE\":[104,105]}\n";
    check(parseJournalJson(journal, entries) && entries.size() == 3,
          "parseJournalJson: 3 objects, junk skipped");
    check(entries.at(0).comm == QStringLiteral("nginx")
              && entries.at(0).message == QStringLiteral("boom")
              && entries.at(0).priority == 3 && entries.at(0).pid == 2196
              && entries.at(0).timestampUsec == 1791907430214266LL,
          "parseJournalJson: fields decoded");
    check(entries.at(2).message == QStringLiteral("hi"),
          "parseJournalJson: byte-array MESSAGE decoded");
    check(journalErrorCount(entries) == 2, "journalErrorCount: priority <= 3");

    JournalSnapshot journal2;
    journal2.available = true;
    journal2.entries = entries;
    journal2.errorCount = 2;
    check(journalSummaryText(journal2).contains(QStringLiteral("2 errors")),
          "journal summary text");

    JournalEntry errA;
    errA.priority = 3;
    errA.comm = QStringLiteral("nginx");
    errA.message = QStringLiteral("connect() failed at 10.0.0.5:8080 attempt 1");
    errA.timestampUsec = 100;
    JournalEntry errB;
    errB.priority = 3;
    errB.comm = QStringLiteral("nginx");
    errB.message = QStringLiteral("connect() failed at 10.0.0.9:9999 attempt 7");
    errB.timestampUsec = 200;
    JournalEntry info;
    info.priority = 6;
    info.comm = QStringLiteral("nginx");
    info.message = QStringLiteral("started");
    info.timestampUsec = 150;
    const QList<ErrorGroup> groups = groupJournalErrors({errA, errB, info});
    check(groups.size() == 1, "error grouping: templates collapse");
    check(groups.at(0).count == 2 && groups.at(0).lastSeenUsec == 200,
          "error group: count + lastSeen");
    check(journalErrorGroupsText(groups).contains(QStringLiteral("x2")),
          "error groups text");

    UnitRow prevUnit;
    prevUnit.name = QStringLiteral("a.service");
    prevUnit.active = QStringLiteral("active");
    prevUnit.sub = QStringLiteral("running");
    UnitRow failedUnit;
    failedUnit.name = QStringLiteral("a.service");
    failedUnit.active = QStringLiteral("failed");
    failedUnit.sub = QStringLiteral("failed");
    UnitRow newUnit;
    newUnit.name = QStringLiteral("b.service");
    newUnit.active = QStringLiteral("active");
    newUnit.sub = QStringLiteral("running");
    const QList<ServiceChange> changes =
        diffServices({prevUnit}, {failedUnit, newUnit}, 1000);
    bool sawFail = false;
    bool sawAppear = false;
    for (const ServiceChange &change : changes) {
        if (change.unit == QStringLiteral("a.service")
            && change.summary.contains(QStringLiteral("failed"))) {
            sawFail = true;
        }
        if (change.unit == QStringLiteral("b.service")
            && change.summary.contains(QStringLiteral("appeared"))) {
            sawAppear = true;
        }
    }
    check(sawFail && sawAppear, "service diff: failure + new unit");
    check(servicesChangesText(changes).contains(QStringLiteral("a.service")),
          "services changes text");

    AlertEngine engine;
    AlertInput input;
    input.appCpuValid = true;
    input.appCpu = 95.0;
    check(engine.update(input, 1).isEmpty(), "alert: no fire before sustained");
    check(engine.update(input, 2).isEmpty(), "alert: still not firing");
    const QList<Alert> fired = engine.update(input, 3);
    check(fired.size() == 1 && fired.at(0).id == QStringLiteral("app_cpu"),
          "alert: fires at sustained count");
    check(engine.active().size() == 1, "alert: active list has one");
    input.appCpu = 10.0;
    engine.update(input, 4);
    check(engine.active().isEmpty(), "alert: resolves on recovery");

    AlertInput servicesInput;
    servicesInput.servicesValid = true;
    servicesInput.servicesFailed = 1;
    const QList<Alert> immediate = engine.update(servicesInput, 5);
    check(immediate.size() == 1
              && immediate.at(0).id == QStringLiteral("services_failed"),
          "alert: service failure fires immediately");

    engine.reset();
    AlertInput memInput;
    memInput.hostMemValid = true;
    memInput.hostMemFreePct = 5.0;
    engine.update(memInput, 6);
    engine.update(memInput, 7);
    check(engine.update(memInput, 8).size() == 1,
          "alert: low memory fires after sustained");

    int status = 0;
    double total = 0;
    double dns = 0;
    double connect = 0;
    double tls = 0;
    double processing = 0;
    double transfer = 0;
    check(parseCurlWriteOut("200 0.175 0.354 0.500 0.830 0.831", status, total,
                            dns, connect, tls, processing, transfer)
              && status == 200
              && std::fabs(dns - 175.0) < 1.0
              && std::fabs(connect - (354.0 - 175.0)) < 1.0
              && std::fabs(tls - (500.0 - 354.0)) < 1.0
              && std::fabs(processing - (830.0 - 500.0)) < 1.0
              && std::fabs(transfer - (831.0 - 830.0)) < 1.0
              && std::fabs(total - 831.0) < 1.0,
          "parseCurlWriteOut: phase decomposition");
    int s2 = 0;
    double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    check(!parseCurlWriteOut(QByteArrayLiteral("garbage"), s2, a, b, c, d, e, f),
          "parseCurlWriteOut: garbage -> false");

    SyntheticSnapshot synthetic;
    ProbeResult up;
    up.url = QStringLiteral("https://x");
    up.success = true;
    up.status = 200;
    up.totalMs = 12;
    ProbeResult down;
    down.url = QStringLiteral("https://y");
    down.success = false;
    down.error = QStringLiteral("connection refused");
    synthetic.results = {up, down};
    synthetic.upCount = 1;
    synthetic.downCount = 1;
    check(syntheticSummaryText(synthetic).contains(QStringLiteral("1 up")),
          "synthetic summary");
    check(syntheticDetailText(synthetic).contains(QStringLiteral("DOWN")),
          "synthetic detail lists down target");

    {
        auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };

        check(diskIsPhysical(QStringLiteral("sda")), "disk: sda physical");
        check(diskIsPhysical(QStringLiteral("nvme0n1")), "disk: nvme0n1 physical");
        check(!diskIsPhysical(QStringLiteral("sda1")), "disk: sda1 partition skipped");
        check(!diskIsPhysical(QStringLiteral("nvme0n1p1")),
              "disk: nvme partition skipped");
        check(!diskIsPhysical(QStringLiteral("loop0")), "disk: loop skipped");
        check(!diskIsPhysical(QStringLiteral("dm-0")), "disk: dm skipped");

        const QByteArray stats = QByteArrayLiteral(
            "   8       0 sda 1000 20 50000 300 2000 40 80000 500 3 800 900\n"
            "   8       1 sda1 900 10 40000 250 1900 30 70000 450 2 700 800\n"
            "   7       0 loop0 0 0 0 0 0 0 0 0 0 0 0\n");
        const QList<DiskCounters> parsed = parseDiskstats(stats);
        check(parsed.size() == 3, "disk: parsed three entries");
        check(parsed.at(0).name == QStringLiteral("sda")
                  && parsed.at(0).reads == 1000
                  && parsed.at(0).sectorsRead == 50000
                  && parsed.at(0).msReading == 300
                  && parsed.at(0).writes == 2000
                  && parsed.at(0).sectorsWritten == 80000
                  && parsed.at(0).msDoingIo == 800
                  && parsed.at(0).weightedMsDoingIo == 900,
              "disk: parsed sda counters");

        const QList<DiskCounters> prev = parseDiskstats(QByteArrayLiteral(
            "   8       0 sda 0 0 0 0 0 0 0 0 0 0 0\n"));
        const QList<DiskCounters> cur = parseDiskstats(QByteArrayLiteral(
            "   8       0 sda 100 0 2000 100 200 0 4000 200 4 500 600\n"));
        const QList<DiskRow> rows = computeDiskRates(prev, cur, 1000.0);
        check(rows.size() == 1 && rows.at(0).name == QStringLiteral("sda"),
              "disk: rate row for physical device only");
        const DiskRow &r = rows.at(0);
        check(near(r.readPerSec, 100.0), "disk: read iops");
        check(near(r.writePerSec, 200.0), "disk: write iops");
        check(near(r.readKbPerSec, 1000.0), "disk: read kB/s");
        check(near(r.writeKbPerSec, 2000.0), "disk: write kB/s");
        check(near(r.readAwaitMs, 1.0), "disk: read await");
        check(near(r.writeAwaitMs, 1.0), "disk: write await");
        check(near(r.aquSz, 0.6), "disk: aqu-sz");
        check(near(r.utilPercent, 50.0), "disk: %util");

        const IoPressure pressure = parsePressureIo(QByteArrayLiteral(
            "some avg10=1.20 avg60=0.50 avg300=0.10 total=1000\n"
            "full avg10=0.30 avg60=0.20 avg300=0.05 total=500\n"));
        check(pressure.available, "disk: pressure available");
        check(near(pressure.someAvg10, 1.20) && near(pressure.someAvg300, 0.10),
              "disk: pressure some fields");
        check(near(pressure.fullAvg300, 0.05), "disk: pressure full field");

        DiskSnapshot snap;
        snap.available = true;
        snap.disks = rows;
        snap.pressure = pressure;
        check(diskSummaryText(snap).contains(QStringLiteral("sda")),
              "disk: summary names busiest device");
        check(diskSummaryText(snap).contains(QStringLiteral("io-pressure")),
              "disk: summary includes io pressure");
        check(diskDetailText(snap).contains(QStringLiteral("sda")),
              "disk: detail lists device");
        DiskSnapshot warm;
        warm.available = true;
        warm.warming = true;
        check(diskSummaryText(warm).contains(QStringLiteral("warming")),
              "disk: warming summary");
    }

    {
        auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };

        const QByteArray snmp = QByteArrayLiteral(
            "Tcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens "
            "AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs "
            "InErrs OutRsts InCsumErrors\n"
            "Tcp: 1 200 120000 -1 100 50 3 1 40 1000 900 9 0 2 0\n"
            "Ip: InReceives\nIp: 55\n");
        const QByteArray netstat = QByteArrayLiteral(
            "TcpExt: TCPTimeouts TCPSynRetrans ListenDrops\n"
            "TcpExt: 12 3 1\nIpExt: X\nIpExt: 7\n");

        const TcpCounters counters = parseTcpCounters(snmp, netstat);
        check(counters.tcp.value(QStringLiteral("OutSegs")) == 900
                  && counters.tcp.value(QStringLiteral("RetransSegs")) == 9
                  && counters.tcp.value(QStringLiteral("CurrEstab")) == 40,
              "tcp: parsed Tcp section by header name");
        check(counters.ext.value(QStringLiteral("TCPTimeouts")) == 12
                  && counters.ext.value(QStringLiteral("ListenDrops")) == 1,
              "tcp: parsed TcpExt section");

        const TcpCounters prev = parseTcpCounters(
            QByteArrayLiteral(
                "Tcp: InSegs OutSegs RetransSegs InErrs OutRsts CurrEstab\n"
                "Tcp: 900 800 6 0 1 40\n"),
            QByteArrayLiteral("TcpExt: TCPTimeouts TCPSynRetrans ListenDrops\n"
                              "TcpExt: 10 1 0\n"));
        const TcpCounters cur = parseTcpCounters(
            QByteArrayLiteral(
                "Tcp: InSegs OutSegs RetransSegs InErrs OutRsts CurrEstab\n"
                "Tcp: 1000 900 9 0 3 42\n"),
            QByteArrayLiteral("TcpExt: TCPTimeouts TCPSynRetrans ListenDrops\n"
                              "TcpExt: 12 3 1\n"));
        const TcpHealth health = computeTcpHealth(prev, cur, 1000.0);
        check(health.available, "tcp: health available");
        check(near(health.inSegsPerSec, 100.0), "tcp: in segs/s");
        check(near(health.outSegsPerSec, 100.0), "tcp: out segs/s");
        check(near(health.retransPerSec, 3.0), "tcp: retrans/s");
        check(near(health.retransPercent, 3.0), "tcp: retrans percent");
        check(near(health.outRstsPerSec, 2.0), "tcp: rsts/s");
        check(near(health.timeoutsPerSec, 2.0), "tcp: timeouts/s");
        check(near(health.synRetransPerSec, 2.0), "tcp: syn retrans/s");
        check(near(health.listenDropsPerSec, 1.0), "tcp: listen drops/s");
        check(health.currEstab == 42, "tcp: current established");
        check(tcpHealthSummaryText(health).contains(QStringLiteral("retrans")),
              "tcp: summary mentions retrans");
        check(tcpHealthDetailText(health).contains(QStringLiteral("syn-retr")),
              "tcp: detail lists syn retrans");
    }

    {
        const QStringList caps =
            decodeCapabilities((1ULL << 21) | (1ULL << 16) | (1ULL << 12));
        check(caps.contains(QStringLiteral("CAP_SYS_ADMIN"))
                  && caps.contains(QStringLiteral("CAP_SYS_MODULE"))
                  && caps.contains(QStringLiteral("CAP_NET_ADMIN")),
              "security: capability decode");

        SecurityInput hardened;
        hardened.capKnown = true;
        hardened.capEff = 0;
        hardened.seccomp = 2;
        hardened.noNewPrivs = true;
        hardened.macConfined = true;
        hardened.macLabel = QStringLiteral("unconfined-userns");
        hardened.isolatedNamespaces = 7;
        hardened.totalNamespaces = 7;
        const SecurityPosture ok = evaluateSecurity(hardened);
        check(ok.available && std::fabs(ok.exposure - 0.2) < 1e-6,
              "security: hardened exposure is low");
        check(ok.grade == QStringLiteral("low"), "security: hardened grade low");

        SecurityInput exposed;
        exposed.capKnown = true;
        exposed.capEff = (1ULL << 21) | (1ULL << 16);
        exposed.seccomp = 0;
        exposed.noNewPrivs = false;
        exposed.macConfined = false;
        exposed.rootUser = true;
        exposed.deletedExe = true;
        exposed.isolatedNamespaces = 0;
        exposed.totalNamespaces = 7;
        const SecurityPosture bad = evaluateSecurity(exposed);
        check(std::fabs(bad.exposure - 4.8) < 1e-6,
              "security: exposed exposure sums correctly");
        check(bad.grade == QStringLiteral("medium"),
              "security: exposed grade medium");
        check(bad.exposedCount >= 6, "security: counts exposed findings");
        check(securitySummaryText(bad).contains(QStringLiteral("exposure")),
              "security: summary text");
        check(securityDetailText(bad).contains(QStringLiteral("CAP_SYS_ADMIN")),
              "security: detail lists capability");
    }

    {
        const LogFields nginx = parseLogLineAuto(QStringLiteral(
            "1.2.3.4 example.com - [10/Oct/2026:13:55:36 +0000] "
            "\"GET /api/v1/items HTTP/1.1\" 200 2326 \"-\" \"curl/8.0\""));
        check(nginx.matched && nginx.parser == QStringLiteral("nginx"),
              "logs: nginx parser matched");
        check(nginx.fields.value(QStringLiteral("code")) == QStringLiteral("200")
                  && nginx.fields.value(QStringLiteral("path"))
                         == QStringLiteral("/api/v1/items"),
              "logs: nginx extracted code/path");

        const LogFields apache = parseLogLineAuto(QStringLiteral(
            "127.0.0.1 - frank [10/Oct/2026:13:55:36 +0000] "
            "\"GET /index.html HTTP/1.0\" 404 512"));
        check(apache.matched
                  && (apache.parser == QStringLiteral("apache")
                      || apache.parser == QStringLiteral("apache2")),
              "logs: apache parser matched");
        check(apache.fields.value(QStringLiteral("code")) == QStringLiteral("404"),
              "logs: apache extracted code");

        const LogFields syslog = parseLogLineAuto(QStringLiteral(
            "<34>1 2026-10-10T22:14:15.003Z host app 1234 ID47 - message body"));
        check(syslog.matched
                  && syslog.parser == QStringLiteral("syslog-rfc5424"),
              "logs: rfc5424 parser matched");
        check(syslog.fields.value(QStringLiteral("ident")) == QStringLiteral("app"),
              "logs: rfc5424 extracted ident");

        const LogFields cri = parseLogLineAuto(QStringLiteral(
            "2026-10-10T22:14:15.003Z stdout F hello from container"));
        check(cri.matched && cri.parser == QStringLiteral("cri"),
              "logs: cri parser matched");
        check(cri.fields.value(QStringLiteral("stream")) == QStringLiteral("stdout"),
              "logs: cri extracted stream");

        const LogFields json = parseLogLineAuto(QStringLiteral(
            "{\"level\":\"error\",\"code\":500,\"path\":\"/x\",\"duration\":12.5}"));
        check(json.matched && json.parser == QStringLiteral("json"),
              "logs: json parser matched");
        check(json.fields.value(QStringLiteral("level")) == QStringLiteral("error"),
              "logs: json extracted level");

        const LogFields logfmt = parseLogLineAuto(QStringLiteral(
            "level=warn code=429 path=/rate msg=\"slow down\""));
        check(logfmt.matched && logfmt.parser == QStringLiteral("logfmt"),
              "logs: logfmt parser matched");
        check(logfmt.fields.value(QStringLiteral("msg")) == QStringLiteral("slow down"),
              "logs: logfmt extracted quoted value");

        QStringList lines;
        lines << QStringLiteral(
                     "1.2.3.4 h - [10/Oct/2026:13:55:36 +0000] \"GET /a HTTP/1.1\" "
                     "200 10 \"-\" \"curl/8.0\"")
              << QStringLiteral(
                     "1.2.3.4 h - [10/Oct/2026:13:55:37 +0000] \"GET /b HTTP/1.1\" "
                     "200 20 \"-\" \"curl/8.0\"")
              << QStringLiteral(
                     "1.2.3.4 h - [10/Oct/2026:13:55:38 +0000] \"GET /a HTTP/1.1\" "
                     "500 5 \"-\" \"curl/8.0\"")
              << QStringLiteral("this line does not match any parser");
        const LogFacets facets = computeLogFacets(lines);
        check(facets.total == 4 && facets.parsed == 3,
              "logs: facets parsed 3/4 lines");
        check(facets.format == QStringLiteral("nginx"),
              "logs: facets dominant format nginx");
        bool saw500 = false;
        bool saw200 = false;
        for (const QPair<QString, int> &c : facets.codes) {
            if (c.first == QStringLiteral("500") && c.second == 1) {
                saw500 = true;
            }
            if (c.first == QStringLiteral("200") && c.second == 2) {
                saw200 = true;
            }
        }
        check(saw200 && saw500, "logs: facets aggregate http codes");
        check(logPipelineSummaryText(facets).contains(QStringLiteral("format")),
              "logs: pipeline summary text");
    }

    {
        const QByteArray otlp = QByteArrayLiteral(
            "{\"resourceSpans\":["
            "{\"resource\":{\"attributes\":[{\"key\":\"service.name\","
            "\"value\":{\"stringValue\":\"frontend\"}}]},"
            "\"scopeSpans\":[{\"spans\":["
            "{\"traceId\":\"t1\",\"spanId\":\"s1\",\"name\":\"GET /checkout\","
            "\"kind\":2,\"startTimeUnixNano\":\"1000000000\","
            "\"endTimeUnixNano\":\"1050000000\",\"status\":{\"code\":1}},"
            "{\"traceId\":\"t1\",\"spanId\":\"s2\",\"parentSpanId\":\"s1\","
            "\"name\":\"GET /pay\",\"kind\":3,\"startTimeUnixNano\":\"1010000000\","
            "\"endTimeUnixNano\":\"1030000000\",\"status\":{\"code\":2}}]}]},"
            "{\"resource\":{\"attributes\":[{\"key\":\"service.name\","
            "\"value\":{\"stringValue\":\"payments\"}}]},"
            "\"scopeSpans\":[{\"spans\":["
            "{\"traceId\":\"t1\",\"spanId\":\"s3\",\"parentSpanId\":\"s2\","
            "\"name\":\"POST /charge\",\"kind\":2,\"startTimeUnixNano\":\"1015000000\","
            "\"endTimeUnixNano\":\"1025000000\",\"status\":{\"code\":1}}]}]}]}");

        bool ok = false;
        const QList<Span> spans = parseOtlpTraces(otlp, &ok);
        check(ok && spans.size() == 3, "traces: parsed three OTLP spans");
        check(spans.at(0).service == QStringLiteral("frontend"),
              "traces: resource service.name applied");
        check(spans.at(1).status == QStringLiteral("error"),
              "traces: error status decoded");

        const TraceSnapshot snapshot = summarizeTraces(spans);
        check(snapshot.spanCount == 3 && snapshot.traceCount == 1,
              "traces: span/trace counts");
        check(snapshot.errorCount == 1, "traces: error count");
        check(snapshot.services.size() == 2, "traces: two services");
        check(snapshot.edges.size() == 1
                  && snapshot.edges.at(0).from == QStringLiteral("frontend")
                  && snapshot.edges.at(0).to == QStringLiteral("payments"),
              "traces: cross-service edge frontend->payments");
        check(snapshot.recent.size() == 1
                  && snapshot.recent.at(0).rootService == QStringLiteral("frontend"),
              "traces: recent trace root service");
        check(std::fabs(snapshot.recent.at(0).durationMs - 50.0) < 1e-6,
              "traces: trace duration 50ms");
        check(snapshot.recent.at(0).hasError, "traces: trace flagged error");
        check(traceSummaryText(snapshot).contains(QStringLiteral("traces")),
              "traces: summary text");
        check(traceServiceMapText(snapshot).contains(QStringLiteral("->")),
              "traces: service map text");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
