#include "MetricStore.h"
#include "Sparkline.h"
#include "IoRateTracker.h"
#include "NetRateTracker.h"
#include "ServicesStatus.h"
#include "JournalStatus.h"

#include <QApplication>
#include <QPixmap>
#include <QThread>

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

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
