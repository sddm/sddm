// SPDX-License-Identifier: GPL-2.0-or-later

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "Seat.h"
#include "SocketServer.h"
#include "Messages.h"
#include "LoginMessages.h"
#include "SocketWriter.h"
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>
using namespace SDDM;
struct SocketPair {
    QLocalServer server;
    QLocalSocket client;
    QLocalSocket *peer = nullptr;
    SocketPair() {
        if (!server.listen(QStringLiteral("display-test-") + QUuid::createUuid().toString(QUuid::Id128)))
            qFatal("socket listen failed");
        client.connectToServer(server.fullServerName());
        if (!client.waitForConnected(1000) || !server.waitForNewConnection(1000))
            qFatal("socket connect failed");
        peer = server.nextPendingConnection();
        if (!peer)
            qFatal("socket missing peer");
    }
};
class DisplayLifecycleTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    Session session;
    QString proof;
    static QByteArray contents(const QString &name) {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }
    void prepare(int passwordDelay, int fingerDelay, const char *fingerMode = "ok") {
        proof = dir.filePath(QUuid::createUuid().toString(QUuid::Id128));
        QFile desktop(dir.filePath("fixture.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(("[Desktop Entry]\nName=Fixture\nType=Application\nExec=display-fixture;" +
                       QString::number(passwordDelay) + ";" + QString::number(fingerDelay) + ";" +
                       QString::fromLatin1(fingerMode) + ";" + proof + "\n")
                          .toUtf8());
        desktop.close();
        mainConfig.Wayland.SessionDir.set({dir.path()});
        session.setTo(Session::WaylandSession, QStringLiteral("fixture.desktop"));
        QVERIFY(session.isValid());
    }
    void race(bool fingerWins, bool lateFinger = false) {
        prepare(fingerWins ? 900 : 100, fingerWins ? 100 : 900);
        Seat seat(QStringLiteral("seat-fixture"));
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded), stopped(&d, &Display::stopped);
        if (lateFinger) {
            d.login(&socket, "fixture", "synthetic", session);
            d.startFingerprint(&socket, "fixture", session);
        } else {
            d.startFingerprint(&socket, "fixture", session);
            d.login(&socket, "fixture", "synthetic", session);
        }
        QTRY_COMPARE(success.size(), 1);
        QTRY_COMPARE(contents(proof + (fingerWins ? "-fp" : "-pw")), QByteArray("approved"));
        QTest::qWait(150);
        QCOMPARE(stopped.size(), 0);
        QCOMPARE(success.size(), 1);
        QVERIFY(contents(proof + (fingerWins ? "-pw" : "-fp")) != "approved");
        QTRY_COMPARE_WITH_TIMEOUT(stopped.size(), 1, 4000);
    }
  private slots:
    void initTestCase() {
        qRegisterMetaType<QLocalSocket *>("QLocalSocket*");
        QVERIFY(dir.isValid());
        mainConfig.ParallelFingerprint.set(true);
        mainConfig.Users.ReuseSession.set(false);
        mainConfig.Users.RememberLastUser.set(false);
        mainConfig.Users.RememberLastSession.set(false);
        mainConfig.Autologin.User.set(QString());
    }
    void oldGreeterGetsOnlyLegacyFramesUntilCapabilityQuery() {
        for (bool enabled : {false, true}) {
            mainConfig.ParallelFingerprint.set(enabled);
            SocketServer server;
            QVERIFY(server.start(QStringLiteral("legacy-fixture")));
            QLocalSocket client;
            client.connectToServer(server.socketAddress());
            QTRY_COMPARE(client.state(), QLocalSocket::ConnectedState);
            SocketWriter(&client) << quint32(GreeterMessages::Connect);
            QTRY_VERIFY(client.bytesAvailable() >= 8);
            QDataStream input(&client);
            LoginReply caps, host;
            QVERIFY(readLoginReply(input, caps));
            QCOMPARE(caps.kind, DaemonMessages::Capabilities);
            QTRY_VERIFY(client.bytesAvailable() >= 22);
            QVERIFY(readLoginReply(input, host));
            QCOMPARE(host.kind, DaemonMessages::HostName);
            QCOMPARE(host.text, QStringLiteral("fixture"));
            QTest::qWait(100);
            QCOMPARE(client.bytesAvailable(), qint64(0));
            SocketWriter(&client) << quint32(GreeterMessages::QueryParallelFingerprint);
            QTRY_VERIFY(client.bytesAvailable() >= 5);
            LoginReply capability;
            QVERIFY(readLoginReply(input, capability));
            QCOMPARE(capability.kind, DaemonMessages::ParallelFingerprint);
            QCOMPARE(capability.active, enabled);
            server.stop();
        }
        mainConfig.ParallelFingerprint.set(true);
    }
    void downgradePreservesOldAndNewGreeterFraming_data() {
        QTest::addColumn<bool>("queried");
        QTest::newRow("old-greeter") << false;
        QTest::newRow("new-greeter") << true;
    }
    void downgradePreservesOldAndNewGreeterFraming() {
        QFETCH(bool, queried);
        testParallelFailure(12);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        prepare(50, 50);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        auto *server = d.findChild<SocketServer *>();
        QVERIFY(server);
        QLocalSocket client;
        client.connectToServer(server->socketAddress());
        QTRY_COMPARE(client.state(), QLocalSocket::ConnectedState);
        SocketWriter(&client) << quint32(GreeterMessages::Connect);
        QTRY_VERIFY(client.bytesAvailable() >= 30);
        QDataStream input(&client);
        LoginReply message;
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::Capabilities);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::HostName);
        if (queried) {
            SocketWriter(&client) << quint32(GreeterMessages::QueryParallelFingerprint);
            QTRY_VERIFY(client.bytesAvailable() >= 5);
            QVERIFY(readLoginReply(input, message));
            QCOMPARE(message.kind, DaemonMessages::ParallelFingerprint);
            QVERIFY(message.active);
        }
        SocketWriter(&client) << quint32(GreeterMessages::Login) << QStringLiteral("fixture") << QStringLiteral("synthetic") << session;
        QTRY_VERIFY_WITH_TIMEOUT(!parallelFingerprintEnabled(), 5000);
        if (queried) {
            QTRY_VERIFY(client.bytesAvailable() >= 5);
            QVERIFY(readLoginReply(input, message));
            QCOMPARE(message.kind, DaemonMessages::ParallelFingerprint);
            QVERIFY(!message.active);
        }
        QTRY_VERIFY(client.bytesAvailable() >= 4);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::LoginFailed);
        QTest::qWait(100);
        QCOMPARE(client.bytesAvailable(), qint64(0));
        for (auto *a : d.findChildren<Auth *>()) QTRY_VERIFY_WITH_TIMEOUT(!a->isActive(), 5000);
        SocketWriter(&client) << quint32(GreeterMessages::Login) << QStringLiteral("fixture") << QStringLiteral("synthetic") << session;
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() >= 4, 5000);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::LoginSucceeded);
        d.stop();
    }
    void unavailableParallelSupportKeepsLegacyPassword_data() {
        QTest::addColumn<int>("reason");
        for (int reason : {1, 2, 3, 4, 5, 7, 8, 14}) QTest::newRow(qPrintable(QString::number(reason))) << reason;
    }
    void unavailableParallelSupportKeepsLegacyPassword() {
        QFETCH(int, reason);
        testParallelFailure(reason);
        QVERIFY(!parallelFingerprintEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(testParallelProbeSettled(), 4000);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(50, 100);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.startFingerprint(sockets.peer, "fixture", session);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 5000);
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        QVERIFY(contents(proof + "-fp").isEmpty());
        d.stop();
        testParallelFailure(0);
    }
    void connectionVerificationFailureDowngradesAndPasswordStillWorks_data() {
        QTest::addColumn<int>("reason");
        for (int reason : {9, 10, 11, 12}) QTest::newRow(qPrintable(QString::number(reason))) << reason;
    }
    void connectionVerificationFailureDowngradesAndPasswordStillWorks() {
        QFETCH(int, reason);
        testParallelFailure(reason);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        prepare(50, 50);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded), failure(&d, &Display::loginFailed);
        d.startFingerprint(sockets.peer, "fixture", session);
        QTRY_VERIFY_WITH_TIMEOUT(!parallelFingerprintEnabled(), 5000);
        QVERIFY(failure.size() >= 1);
        for (auto *a : d.findChildren<Auth *>()) QTRY_VERIFY_WITH_TIMEOUT(!a->isActive(), 5000);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 5000);
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        QVERIFY(contents(proof + "-fp").isEmpty());
        d.stop();
        testParallelFailure(0);
    }
    void stalledProbeLeavesEventLoopAndLegacyPasswordAvailable() {
        testParallelFailure(15);
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(!parallelFingerprintEnabled());
        QVERIFY(elapsed.elapsed() < 300);
        prepare(50, 50);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded);
        bool timerFired = false;
        QTimer::singleShot(100, this, [&] { timerFired = true; });
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_VERIFY_WITH_TIMEOUT(timerFired, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 1000);
        QVERIFY(elapsed.elapsed() < 1500);
        QTRY_VERIFY_WITH_TIMEOUT(testParallelProbeSettled(), 4000);
        QVERIFY(!parallelFingerprintEnabled());
        d.stop();
        testParallelFailure(0);
    }
    void lateProbeCannotRaceLegacyPassword() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(1000, 50);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        d.startFingerprint(sockets.peer, "fixture", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 4000);
        QVERIFY(contents(proof + "-fp").isEmpty());
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        d.stop();
        testParallelFailure(0);
    }
    void probeReadyIsWithheldUntilLegacyFailure() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(800, 50, "password-fail");
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded), failure(&d, &Display::loginFailed);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTest::qWait(500);
        QVERIFY(!parallelFingerprintEnabled());
        // A stale/unsolicited request is ignored while capability is withheld.
        d.startFingerprint(sockets.peer, "fixture", session);
        QVERIFY(contents(proof + "-fp").isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(failure.size() >= 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        d.startFingerprint(sockets.peer, "fixture", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 4000);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        d.stop();
    }
    void failedLegacyDisconnectKeepsDisplayForFingerprintRetry() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(800, 50, "password-fail-disconnect");
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        Auth *legacy = nullptr;
        for (auto *auth : d.findChildren<Auth *>()) if (!auth->fingerprint()) legacy = auth;
        QVERIFY(legacy);
        QSignalSpy errors(legacy, &Auth::error);
        QSignalSpy success(&d, &Display::loginSucceeded), failure(&d, &Display::loginFailed), stopped(&d, &Display::stopped);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_VERIFY_WITH_TIMEOUT(failure.size() >= 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        QCOMPARE(stopped.size(), 0);
        QCOMPARE(errors.size(), 0);
        d.startFingerprint(sockets.peer, "fixture", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 4000);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        d.stop();
    }
    void negotiatedGreeterReceivesReadyAfterLegacyFailure() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(800, 50, "password-fail");
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        auto *server = d.findChild<SocketServer *>();
        QVERIFY(server);
        QLocalSocket client;
        client.connectToServer(server->socketAddress());
        QTRY_COMPARE(client.state(), QLocalSocket::ConnectedState);
        SocketWriter(&client) << quint32(GreeterMessages::Connect);
        QTRY_VERIFY(client.bytesAvailable() >= 30);
        QDataStream input(&client);
        LoginReply message;
        QVERIFY(readLoginReply(input, message));
        QVERIFY(readLoginReply(input, message));
        SocketWriter(&client) << quint32(GreeterMessages::QueryParallelFingerprint);
        QTRY_VERIFY(client.bytesAvailable() >= 5);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::ParallelFingerprint);
        QVERIFY(!message.active);
        SocketWriter(&client) << quint32(GreeterMessages::Login) << QStringLiteral("fixture") << QStringLiteral("synthetic") << session;
        QTest::qWait(500);
        QVERIFY(!parallelFingerprintEnabled());
        QCOMPARE(client.bytesAvailable(), qint64(0));
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() >= 9, 4000);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::LoginFailed);
        QVERIFY(readLoginReply(input, message));
        QCOMPARE(message.kind, DaemonMessages::ParallelFingerprint);
        QVERIFY(message.active);
        QSignalSpy success(&d, &Display::loginSucceeded);
        SocketWriter(&client) << quint32(GreeterMessages::StartFingerprint) << QStringLiteral("fixture") << session;
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 4000);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        d.stop();
    }
    void unsolicitedDifferentSelectionCannotStartReaderDuringLegacyLogin() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(800, 50);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.login(sockets.peer, "old-user", "synthetic", session);
        QTest::qWait(500);
        QVERIFY(!parallelFingerprintEnabled());
        d.startFingerprint(sockets.peer, "other-user", session);
        d.startFingerprint(sockets.peer, QString(), session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        // Even after publication, this display cannot race its live session.
        d.startFingerprint(sockets.peer, "other-user", session);
        QTest::qWait(100);
        QVERIFY(contents(proof + "-fp").isEmpty());
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        d.stop();
    }
    void releasedLegacySessionAllowsAnotherDisplayButIsLatched() {
        testParallelFailure(16);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(600, 50);
        const QString firstProof = proof;
        Seat firstSeat("seat-one");
        Display first(&firstSeat, Display::WaylandDisplayServerType);
        QVERIFY(first.start());
        SocketPair firstSockets;
        QSignalSpy firstSuccess(&first, &Display::loginSucceeded);
        first.login(firstSockets.peer, "fixture", "synthetic", session);
        QTRY_COMPARE_WITH_TIMEOUT(firstSuccess.size(), 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        first.startFingerprint(firstSockets.peer, "fixture", session);
        first.login(firstSockets.peer, "fixture", "synthetic", session);
        QTest::qWait(100);
        QCOMPARE(firstSuccess.size(), 1);
        QVERIFY(contents(firstProof + "-fp").isEmpty());
        prepare(50, 50);
        Seat secondSeat("seat-two");
        Display second(&secondSeat, Display::WaylandDisplayServerType);
        QVERIFY(second.start());
        SocketPair secondSockets;
        QSignalSpy secondSuccess(&second, &Display::loginSucceeded);
        second.startFingerprint(secondSockets.peer, "fixture", session);
        QTRY_COMPARE_WITH_TIMEOUT(secondSuccess.size(), 1, 4000);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        second.stop();
        first.stop();
    }
    void disconnectedGreeterIsRemovedBeforeCapabilityBroadcast() {
        SocketServer server;
        QVERIFY(server.start(QStringLiteral("disconnect-fixture")));
        QLocalSocket client;
        client.connectToServer(server.socketAddress());
        QTRY_COMPARE(client.state(), QLocalSocket::ConnectedState);
        SocketWriter(&client) << quint32(GreeterMessages::QueryParallelFingerprint);
        QTRY_VERIFY(client.bytesAvailable() >= 5);
        client.disconnectFromServer();
        QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        server.stop();
    }
    void preHelloTimeoutDowngradesAndPasswordStillWorks() {
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        prepare(50, 50, "no-hello");
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded), failure(&d, &Display::loginFailed);
        d.startFingerprint(sockets.peer, "fixture", session);
        QTRY_VERIFY_WITH_TIMEOUT(!parallelFingerprintEnabled(), 7000);
        QVERIFY(failure.size() >= 1);
        for (auto *a : d.findChildren<Auth *>()) QTRY_VERIFY_WITH_TIMEOUT(!a->isActive(), 3000);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 5000);
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        d.stop();
        testParallelFailure(0);
    }
    void oldKernelExecFlagFallbackStillWorks() {
        testParallelFailure(6);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        testParallelFailure(0);
    }
    void lateParallelSuccessAfterDowngradeCannotStartSession() {
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        prepare(3000, 3000);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QSignalSpy success(&d, &Display::loginSucceeded), failure(&d, &Display::loginFailed);
        d.startFingerprint(sockets.peer, "fixture", session);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTest::qWait(100);
        Auth *finger = nullptr;
        for (auto *auth : d.findChildren<Auth *>()) if (auth->fingerprint()) finger = auth;
        QVERIFY(finger);
        QVERIFY(finger->parallelAttempt());
        disableParallelRuntime();
        QVERIFY(!parallelFingerprintEnabled());
        emit finger->authentication(QStringLiteral("fixture"), true);
        QCOMPARE(success.size(), 0);
        QVERIFY(failure.size() >= 1);
        QTRY_VERIFY_WITH_TIMEOUT(!finger->isActive(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!d.findChildren<Auth *>().at(0)->isActive(), 3000);
        prepare(50, 50);
        d.login(sockets.peer, "fixture", "synthetic", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 5000);
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        d.stop();
        testParallelFailure(0);
    }
    void init() { testParallelFailure(0); QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000); }
    void reuseSessionDisablesParallelChannelButKeepsPassword() {
        mainConfig.Users.ReuseSession.set(true);
        QVERIFY(!parallelFingerprintEnabled());
        prepare(100, 100);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.startFingerprint(&socket, "fixture", session);
        QTest::qWait(250);
        QVERIFY(contents(proof + "-fp").isEmpty());
        d.login(&socket, "fixture", "synthetic", session);
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(contents(proof + "-fp").isEmpty());
        d.stop();
        mainConfig.Users.ReuseSession.set(false);
    }
    void passwordWinRetainsSessionUntilWinningHelperExits() {
        race(false);
    }
    void fingerprintWinRetainsSessionUntilWinningHelperExits() {
        race(true);
    }
    void indicatorWaitsForVerifiedReaderAndRejectsStaleSignals() {
        prepare(100, 4000);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        d.startFingerprint(sockets.peer, "fixture", session);
        Auth *finger = nullptr, *password = nullptr;
        for (auto *auth : d.findChildren<Auth *>()) {
            if (auth->fingerprint()) finger = auth; else password = auth;
        }
        QVERIFY(finger); QVERIFY(password); QVERIFY(finger->isActive());
        QTest::qWait(150);
        QCOMPARE(sockets.client.bytesAvailable(), qint64(0));
        const auto marker = QStringLiteral("\036FPRINTD_VERIFY_READY_V1");
        emit password->info(marker, Auth::INFO_UNKNOWN);
        QTest::qWait(30);
        QCOMPARE(sockets.client.bytesAvailable(), qint64(0));
        emit finger->info(marker, Auth::INFO_UNKNOWN);
        QTRY_VERIFY(sockets.client.bytesAvailable() >= 5);
        QDataStream input(&sockets.client);
        LoginReply reply;
        QVERIFY(readLoginReply(input, reply));
        QCOMPARE(reply.kind, DaemonMessages::FingerprintActive);
        QVERIFY(reply.active);
        d.startFingerprint(sockets.peer, QString(), session);
        QTRY_VERIFY(sockets.client.bytesAvailable() >= 5);
        QVERIFY(readLoginReply(input, reply));
        QCOMPARE(reply.kind, DaemonMessages::FingerprintActive);
        QVERIFY(!reply.active);
        emit finger->info(marker, Auth::INFO_UNKNOWN);
        QTest::qWait(100);
        while (sockets.client.bytesAvailable()) {
            QVERIFY(readLoginReply(input, reply));
            QCOMPARE(reply.kind, DaemonMessages::FingerprintActive);
            QVERIFY(!reply.active);
        }
        d.stop();
    }
    void lateReaderStartKeepsPasswordAlive() {
        race(false, true);
    }
    void differentUserPasswordCannotReplaceWaitingFingerprint() {
        prepare(100, 300);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy failure(&d, &Display::loginFailed), success(&d, &Display::loginSucceeded);
        d.startFingerprint(&socket, "fixture", session);
        d.login(&socket, "other-user", "synthetic", session);
        QCOMPARE(failure.size(), 1);
        QTRY_COMPARE(success.size(), 1);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        QVERIFY(contents(proof + "-pw").isEmpty());
        d.stop();
    }
    void stopCancelsPendingChannelsWithoutLogin() {
        prepare(800, 800);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded), stopped(&d, &Display::stopped);
        d.startFingerprint(&socket, "fixture", session);
        d.login(&socket, "fixture", "synthetic", session);
        d.stop();
        QCOMPARE(stopped.size(), 1);
        QTest::qWait(1000);
        QCOMPARE(success.size(), 0);
        QVERIFY(contents(proof + "-fp") != "approved");
        QVERIFY(contents(proof + "-pw") != "approved");
    }
    void clearedSelectionCancelsPendingChannels_data() {
        QTest::addColumn<QString>("selectedUser");
        QTest::addColumn<bool>("validSession");
        QTest::newRow("empty-user") << QString() << true;
        QTest::newRow("invalid-session") << QStringLiteral("fixture") << false;
        QTest::newRow("greeter-account") << QStringLiteral("sddm") << true;
    }
    void clearedSelectionCancelsPendingChannels() {
        QFETCH(QString, selectedUser);
        QFETCH(bool, validSession);
        prepare(500, 500);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.startFingerprint(&socket, "fixture", session);
        d.login(&socket, "fixture", "synthetic", session);
        d.startFingerprint(&socket, selectedUser, validSession ? session : Session());
        QTest::qWait(800);
        QCOMPARE(success.size(), 0);
        QVERIFY(contents(proof + "-fp") != "approved");
        QVERIFY(contents(proof + "-pw") != "approved");
        for (auto *auth : d.findChildren<Auth *>())
            QVERIFY(!auth->isActive());
        prepare(100, 100);
        d.startFingerprint(&socket, "fixture", session);
        QTRY_COMPARE(success.size(), 1);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        d.stop();
    }
    void disconnectCancelsPendingChannels() {
        prepare(800, 800);
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.startFingerprint(&socket, "fixture", session);
        d.login(&socket, "fixture", "synthetic", session);
        auto *server = d.findChild<SocketServer *>();
        QVERIFY(server);
        emit server->disconnected(&socket);
        QTest::qWait(1000);
        QCOMPARE(success.size(), 0);
        QVERIFY(contents(proof + "-fp") != "approved");
        QVERIFY(contents(proof + "-pw") != "approved");
        d.stop();
    }
    void selectionChangeRejectsOldHelpersAndStartsNewSelection() {
        prepare(800, 800);
        QString oldProof = proof;
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        QSignalSpy success(&d, &Display::loginSucceeded);
        d.startFingerprint(&socket, "old-user", session);
        d.login(&socket, "old-user", "synthetic", session);
        prepare(100, 100);
        d.startFingerprint(&socket, "new-user", session);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 5000);
        QTRY_COMPARE(contents(proof + "-fp"), QByteArray("approved"));
        QVERIFY(contents(oldProof + "-fp") != "approved");
        QVERIFY(contents(oldProof + "-pw") != "approved");
        d.stop();
    }
    void fingerprintFailureRetriesAndPasswordRemainsAvailable() {
        prepare(100, 80, "fail");
        Seat seat("seat-fixture");
        Display d(&seat, Display::WaylandDisplayServerType);
        QVERIFY(d.start());
        SocketPair sockets;
        QLocalSocket &socket = *sockets.peer;
        Auth *fp = nullptr;
        for (auto *a : d.findChildren<Auth *>())
            if (a->fingerprint())
                fp = a;
        QVERIFY(fp);
        QSignalSpy finished(fp, &Auth::finished), success(&d, &Display::loginSucceeded), stopped(&d, &Display::stopped);
        d.startFingerprint(&socket, "fixture", session);
        QTRY_VERIFY_WITH_TIMEOUT(finished.size() >= 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(parallelFingerprintEnabled(), 4000);
        QCOMPARE(stopped.size(), 0);
        d.login(&socket, "fixture", "synthetic", session);
        QTRY_COMPARE(success.size(), 1);
        QTRY_COMPARE(contents(proof + "-pw"), QByteArray("approved"));
        d.stop();
    }
};
int main(int argc, char **argv) {
    DaemonApp app(argc, argv);
    DisplayLifecycleTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DisplayLifecycleTest.moc"
