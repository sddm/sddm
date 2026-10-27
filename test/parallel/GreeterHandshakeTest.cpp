// SPDX-License-Identifier: GPL-2.0-or-later

#include "Configuration.h"
#include "GreeterProxy.h"
#include "LoginMessages.h"
#include "SessionModel.h"
#include "SocketWriter.h"
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace SDDM;

// Real GreeterProxy and production SocketWriter; no PAM, sensor or session launch.
class GreeterHandshakeTest : public QObject {
    Q_OBJECT
  private slots:
    void startup_data() {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("parallel-enabled") << true;
        QTest::newRow("parallel-disabled") << false;
    }

    void startup() {
        QFETCH(bool, enabled);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString sessionsPath = directory.filePath("sessions");
        QVERIFY(QDir().mkdir(sessionsPath));
        QFile desktop(sessionsPath + "/fixture.desktop");
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nName=Fixture\nExec=/usr/bin/true\nTryExec=/usr/bin/true\nType=Application\n");
        desktop.close();
        mainConfig.X11.SessionDir.set(QStringList{sessionsPath});
        mainConfig.Wayland.SessionDir.set(QStringList{sessionsPath});
        SessionModel sessions;
        QVERIFY(sessions.rowCount() > 0);

        QLocalServer server;
        QVERIFY(server.listen(directory.filePath("socket")));
        GreeterProxy proxy(server.fullServerName());
        proxy.setSessionModel(&sessions);
        QSignalSpy capabilityChanged(&proxy, &GreeterProxy::parallelFingerprintChanged);
        QSignalSpy readerActive(&proxy, &GreeterProxy::fingerprintActiveChanged);
        QSignalSpy success(&proxy, &GreeterProxy::loginSucceeded);
        // Mirrors the theme's late-capability callback, through the real proxy.
        connect(&proxy, &GreeterProxy::parallelFingerprintChanged, &proxy, [&] {
            if (proxy.parallelFingerprint())
                proxy.startFingerprint(QStringLiteral("fixture-user"), 0);
        });
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QLocalSocket> peer(server.nextPendingConnection());
        QTRY_VERIFY(peer->bytesAvailable() >= 4);
        {
            QDataStream input(peer.data());
            LoginRequest request;
            QVERIFY(readLoginRequest(input, request));
            QCOMPARE(request.kind, GreeterMessages::Connect);
        }

        QTRY_VERIFY(peer->bytesAvailable() >= 4);
        {
            QDataStream input(peer.data());
            LoginRequest request;
            QVERIFY(readLoginRequest(input, request));
            QCOMPARE(request.kind, GreeterMessages::QueryParallelFingerprint);
            QCOMPARE(peer->bytesAvailable(), 0);
        }
        // A legacy daemon discards this payload-free query; a new daemon replies.
        SocketWriter(peer.data()) << quint32(DaemonMessages::Capabilities) << quint32(Capability::Reboot);
        SocketWriter(peer.data()) << quint32(DaemonMessages::ParallelFingerprint) << enabled;
        SocketWriter(peer.data()) << quint32(DaemonMessages::HostName) << QStringLiteral("fixture-host");
        QTRY_COMPARE_WITH_TIMEOUT(capabilityChanged.count(), 1, 500);
        QCOMPARE(proxy.parallelFingerprint(), enabled);
        QTRY_COMPARE_WITH_TIMEOUT(proxy.hostName(), QStringLiteral("fixture-host"), 500);
        QVERIFY(proxy.canReboot());

        if (enabled) {
            QTRY_VERIFY(peer->bytesAvailable() > 0);
            QDataStream input(peer.data());
            LoginRequest request;
            QVERIFY(readLoginRequest(input, request));
            QCOMPARE(request.kind, GreeterMessages::StartFingerprint);
            QCOMPARE(request.user, QStringLiteral("fixture-user"));
            QVERIFY(request.password.isEmpty());
            QVERIFY(request.session.isValid());
        } else {
            QCOMPARE(peer->bytesAvailable(), 0);
        }

        // A theme clearing its account or session must reach the daemon, so
        // the previous selection cannot keep authenticating in the background.
        if (enabled) {
            for (const auto &selection : {qMakePair(QString(), 0), qMakePair(QStringLiteral("fixture-user"), -1)}) {
                proxy.startFingerprint(selection.first, selection.second);
                QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0, 500);
                QDataStream input(peer.data());
                LoginRequest request;
                QVERIFY(readLoginRequest(input, request));
                QCOMPARE(request.kind, GreeterMessages::StartFingerprint);
                QCOMPARE(request.user, selection.first);
                QCOMPARE(request.session.isValid(), selection.second >= 0);
            }
        }

        // A password request remains independent of the fingerprint request.
        proxy.login(QStringLiteral("fixture-user"), QStringLiteral("synthetic-password"), 0);
        QTRY_VERIFY(peer->bytesAvailable() > 0);
        {
            QDataStream input(peer.data());
            LoginRequest request;
            QVERIFY(readLoginRequest(input, request));
            QCOMPARE(request.kind, GreeterMessages::Login);
            QCOMPARE(request.user, QStringLiteral("fixture-user"));
            QCOMPARE(request.password, QStringLiteral("synthetic-password"));
        }

        SocketWriter(peer.data()) << quint32(DaemonMessages::FingerprintActive) << true;
        SocketWriter(peer.data()) << quint32(DaemonMessages::FingerprintActive) << false;
        SocketWriter(peer.data()) << quint32(DaemonMessages::LoginSucceeded);
        QTRY_COMPARE_WITH_TIMEOUT(readerActive.count(), 2, 500);
        QCOMPARE(readerActive.at(0).at(0).toBool(), true);
        QCOMPARE(readerActive.at(1).at(0).toBool(), false);
        QTRY_COMPARE_WITH_TIMEOUT(success.count(), 1, 500);
    }
};

QTEST_GUILESS_MAIN(GreeterHandshakeTest)
#include "GreeterHandshakeTest.moc"
