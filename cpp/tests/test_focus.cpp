#include "FocusSampler.h"
#include "FocusedWindowProvider.h"
#include "GnomePidFileProvider.h"
#include "LinuxProcess.h"
#include "ProcessIdentity.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryFile>

#include <unistd.h>

#include <cstdio>

namespace {

class FakeProvider : public FocusedWindowProvider {
public:
    explicit FakeProvider(int pid) : m_pid(pid) {}

    QString name() const override { return QStringLiteral("fake"); }
    int focusedPid() const override { return m_pid; }

private:
    int m_pid;
};

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    const int self = static_cast<int>(::getpid());
    const int deadPid = 1 << 30;

    check(linuxproc::exists(self), "exists(self) is true");
    check(!linuxproc::exists(deadPid), "exists(bogus pid) is false");
    check(!linuxproc::exists(-1), "exists(-1) is false");
    check(!linuxproc::comm(self).isEmpty(), "comm(self) is not empty");
    check(linuxproc::comm(deadPid).isEmpty(), "comm(bogus pid) is empty");
    check(linuxproc::startTime(self) > 0, "startTime(self) > 0");
    check(linuxproc::identity(self).isValid(), "identity(self) is valid");
    check(!linuxproc::identity(deadPid).isValid(), "identity(bogus pid) invalid");

    QTemporaryFile tmp;
    QString pidFilePath;
    if (tmp.open()) {
        tmp.write(QByteArray::number(self));
        tmp.write("\n");
        tmp.flush();
        pidFilePath = tmp.fileName();
    }

    GnomePidFileProvider fileProvider(pidFilePath);
    check(fileProvider.focusedPid() == self, "pid-file provider resolves live pid");

    GnomePidFileProvider missingProvider(
        QStringLiteral("/nonexistent/debug-overlay-active-pid"));
    check(missingProvider.focusedPid() == -1, "missing pid file -> -1");

    GnomePidFileProvider emptyPathProvider{QString()};
    check(emptyPathProvider.focusedPid() == -1, "empty path -> -1");

    FakeProvider liveProvider(self);
    FocusSampler liveSampler(&liveProvider);
    const FocusSample live = liveSampler.sample();
    check(live.available, "sampler available for live pid");
    check(live.identity.pid == self, "sampler reports correct pid");
    check(live.appName == linuxproc::comm(self), "sampler reports comm name");
    check(live.providerName == QStringLiteral("fake"), "sampler reports provider name");

    FakeProvider deadProvider(deadPid);
    FocusSampler deadSampler(&deadProvider);
    check(!deadSampler.sample().available, "sampler unavailable for dead pid");

    FakeProvider noneProvider(-1);
    FocusSampler noneSampler(&noneProvider);
    check(!noneSampler.sample().available, "sampler unavailable for -1");

    FocusSampler nullSampler(nullptr);
    check(!nullSampler.sample().available, "sampler unavailable without provider");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
