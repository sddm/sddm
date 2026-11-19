// SPDX-License-Identifier: GPL-2.0-or-later

#include "AuthMessages.h"
#include "Constants.h"
#include "SafeDataStream.h"
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace SDDM;
static QString helper;
class ReadyHelperTest : public QObject {
    Q_OBJECT
    static void write(const QString &file, const QByteArray &data) {
        QFile f(file);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("fixture write failed");
        f.write(data);
    }
    void trial(bool approve, bool denyAccount, bool malformed = false, bool parallel = true) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir(dir.path()).mkdir(QStringLiteral("pam"));
        QVERIFY(QFile::copy(helper, dir.filePath(QStringLiteral("helper"))));
        QVERIFY(QFile::setPermissions(dir.filePath(QStringLiteral("helper")), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(dir.filePath(QStringLiteral("passwd")), "root:x:0:0:Fixture:/tmp/fixture:/bin/sh\n");
        write(dir.filePath(QStringLiteral("group")), "root:x:0:\n");
        QByteArray policy = "auth required pam_permit.so\naccount required ";
        policy += denyAccount ? "pam_deny.so\n" : "pam_permit.so\n";
        policy += "session required pam_exec.so /usr/bin/touch /tmp/fixture/session-opened\n";
        const QByteArray module = qgetenv("READY_PAM_MODULE");
        QVERIFY(!module.isEmpty());
        policy = "auth required " + module + " notify-ready max-tries=1 timeout=5\n" + policy.mid(policy.indexOf("account required"));
        write(dir.filePath(QStringLiteral("pam/") + QStringLiteral(PAM_FINGERPRINT_SERVICE)), policy);
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath(QStringLiteral("socket"))));
        QStringList args = {QStringLiteral("--unshare-user"),
                            QStringLiteral("--uid"),
                            QStringLiteral("0"),
                            QStringLiteral("--gid"),
                            QStringLiteral("0"),
                            QStringLiteral("--unshare-pid"),
                            QStringLiteral("--unshare-net"),
                            QStringLiteral("--unshare-ipc"),
                            QStringLiteral("--unshare-uts"),
                            QStringLiteral("--die-with-parent"),
                            QStringLiteral("--ro-bind"),
                            QStringLiteral("/"),
                            QStringLiteral("/"),
                            QStringLiteral("--proc"),
                            QStringLiteral("/proc"),
                            QStringLiteral("--dev"),
                            QStringLiteral("/dev"),
                            QStringLiteral("--tmpfs"),
                            QStringLiteral("/tmp"),
                            QStringLiteral("--bind"),
                            dir.path(),
                            QStringLiteral("/tmp/fixture"),
                            QStringLiteral("--ro-bind"),
                            dir.filePath(QStringLiteral("pam")),
                            QStringLiteral("/etc/pam.d"),
                            QStringLiteral("--ro-bind"),
                            dir.filePath(QStringLiteral("passwd")),
                            QStringLiteral("/etc/passwd"),
                            QStringLiteral("--ro-bind"),
                            dir.filePath(QStringLiteral("group")),
                            QStringLiteral("/etc/group"),
                            QStringLiteral("/tmp/fixture/helper"),
                            QStringLiteral("--socket"),
                            QStringLiteral("/tmp/fixture/socket"),
                            QStringLiteral("--id"),
                            QStringLiteral("1"),
                            QStringLiteral("--user"),
                            QStringLiteral("root"),
                            QStringLiteral("--start"),
                            QStringLiteral("/usr/bin/true")};
        if (parallel)
            args << QStringLiteral("--parallel-auth-v2") << QStringLiteral("--fingerprint");
        QProcess process;
        process.start(QStringLiteral("/usr/bin/bwrap"), args);
        QVERIFY(process.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections() || process.state() == QProcess::NotRunning, 3000);
        QVERIFY2(server.hasPendingConnections(), process.readAllStandardError().constData());
        auto *socket = server.nextPendingConnection();
        bool authenticated = false;
        bool sawReady = false;
        for (int i = 0; i < 30 && process.state() != QProcess::NotRunning; i++) {
            if (!socket->bytesAvailable() && !socket->waitForReadyRead(1000)) {
                QCoreApplication::processEvents();
                continue;
            }
            SafeDataStream stream(socket);
            stream.receive();
            Msg msg = MSG_UNKNOWN;
            stream >> msg;
            if (msg == (parallel ? HELLO_V2 : HELLO)) {
                qint64 id;
                stream >> id;
                QCOMPARE(id, qint64(1));
            } else if (msg == INFO) {
                QString text;
                Auth::Info kind;
                stream >> text >> kind;
                if (text == QStringLiteral("\036FPRINTD_VERIFY_READY_V1")) sawReady = true;
            } else if (msg == ERROR) {
                QString text;
                Auth::Error kind;
                stream >> text >> kind;
            } else if (msg == AUTHENTICATED) {
                QString user;
                stream >> user;
                if (user.isEmpty())
                    break;
                authenticated = true;
                QVERIFY(sawReady);
                QProcessEnvironment env;
                env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
                env.insert(QStringLiteral("XDG_SESSION_TYPE"), QStringLiteral("wayland"));
                env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
                stream.reset();
                stream << AUTHENTICATED;
                if (!malformed) {
                    if (parallel)
                        stream << approve;
                    stream << env << QByteArray();
                }
                stream.send();
            } else if (msg == SESSION_STATUS) {
                bool ok;
                stream >> ok;
                stream.reset();
                stream << SESSION_STATUS;
                stream.send();
            } else
                QFAIL("Unexpected helper protocol message");
        }
        if (process.state() != QProcess::NotRunning && !process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished();
            QFAIL("helper did not finish");
        }
        QCOMPARE(authenticated, !denyAccount);
        QCOMPARE(QFile::exists(dir.filePath(QStringLiteral("session-opened"))), approve && !denyAccount && !malformed);
        if (!approve || denyAccount || malformed)
            QCOMPARE(process.exitCode(), int(Auth::HELPER_AUTH_ERROR));
    }
  private slots:
    void actualFingerprintPamReadinessReachesHelper() {
        trial(true, false);
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2)
        return 2;
    helper = QString::fromLocal8Bit(argv[1]);
    ReadyHelperTest test;
    return QTest::qExec(&test, 1, argv);
}
#include "ReadyHelperTest.moc"
