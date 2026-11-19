// SPDX-License-Identifier: GPL-2.0-or-later

#include "LoginMessages.h"
#include "SocketWriter.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>
using namespace SDDM;
class LoginMessagesTest : public QObject {
    Q_OBJECT
    void fragmentedRequest(GreeterMessages type) {
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out << quint32(type) << QStringLiteral("fixture-user");
        if (type == GreeterMessages::Login)
            out << QStringLiteral("synthetic-password");
        out << Session();
        for (qsizetype split = 1; split < packet.size(); ++split) {
            QTemporaryDir dir;
            QLocalServer server;
            QVERIFY(server.listen(dir.filePath(QStringLiteral("socket"))));
            QLocalSocket client;
            client.connectToServer(server.fullServerName());
            QVERIFY(client.waitForConnected());
            QVERIFY(server.waitForNewConnection());
            auto *peer = server.nextPendingConnection();
            client.write(packet.left(split));
            client.waitForBytesWritten();
            QVERIFY(peer->waitForReadyRead());
            {
                QDataStream in(peer);
                LoginRequest request;
                QVERIFY(!readLoginRequest(in, request));
                QVERIFY(request.user.isEmpty());
            }
            client.write(packet.mid(split));
            client.waitForBytesWritten();
            QVERIFY(peer->waitForReadyRead());
            {
                QDataStream in(peer);
                LoginRequest request;
                QVERIFY(readLoginRequest(in, request));
                QCOMPARE(request.kind, type);
                QCOMPARE(request.user, QStringLiteral("fixture-user"));
                if (type == GreeterMessages::Login)
                    QCOMPARE(request.password, QStringLiteral("synthetic-password"));
                else
                    QVERIFY(request.password.isEmpty());
            }
        }
    }
  private slots:
    void productionWriterFlags_data() {
        QTest::addColumn<quint32>("kind");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("availability-true") << quint32(DaemonMessages::ParallelFingerprint) << true;
        QTest::newRow("availability-false") << quint32(DaemonMessages::ParallelFingerprint) << false;
        QTest::newRow("active-true") << quint32(DaemonMessages::FingerprintActive) << true;
        QTest::newRow("active-false") << quint32(DaemonMessages::FingerprintActive) << false;
    }
    void productionWriterFlags() {
        QFETCH(quint32, kind);
        QFETCH(bool, enabled);
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("socket")));
        QLocalSocket client;
        client.connectToServer(server.fullServerName());
        QVERIFY(client.waitForConnected());
        QVERIFY(server.waitForNewConnection());
        auto *peer = server.nextPendingConnection();
        SocketWriter(&client) << kind << enabled;
        SocketWriter(&client) << quint32(DaemonMessages::HostName) << QStringLiteral("fixture-host");
        SocketWriter(&client) << quint32(DaemonMessages::LoginSucceeded);
        QVERIFY(peer->waitForReadyRead());
        QDataStream in(peer);
        LoginReply flag, host, success;
        QVERIFY(readLoginReply(in, flag));
        QCOMPARE(flag.kind, DaemonMessages(kind));
        QCOMPARE(flag.active, enabled);
        QVERIFY(readLoginReply(in, host));
        QCOMPARE(host.kind, DaemonMessages::HostName);
        QCOMPARE(host.text, QStringLiteral("fixture-host"));
        QVERIFY(readLoginReply(in, success));
        QCOMPARE(success.kind, DaemonMessages::LoginSucceeded);
        QCOMPARE(peer->bytesAvailable(), 0);
    }
    void fragmentedFingerprintRequest() {
        fragmentedRequest(GreeterMessages::StartFingerprint);
    }
    void fragmentedPasswordRequest() {
        fragmentedRequest(GreeterMessages::Login);
    }
    void fragmentedStatusDoesNotDisableOrEnablePrematurely() {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath(QStringLiteral("socket"))));
        QLocalSocket client;
        client.connectToServer(server.fullServerName());
        QVERIFY(client.waitForConnected());
        QVERIFY(server.waitForNewConnection());
        auto *peer = server.nextPendingConnection();
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out << quint32(DaemonMessages::ParallelFingerprint) << true;
        client.write(packet.left(4));
        client.waitForBytesWritten();
        QVERIFY(peer->waitForReadyRead());
        {
            QDataStream in(peer);
            LoginReply r;
            QVERIFY(!readLoginReply(in, r));
            QVERIFY(!r.active);
        }
        client.write(packet.mid(4));
        client.waitForBytesWritten();
        QVERIFY(peer->waitForReadyRead());
        {
            QDataStream in(peer);
            LoginReply r;
            QVERIFY(readLoginReply(in, r));
            QVERIFY(r.active);
            QCOMPARE(r.kind, DaemonMessages::ParallelFingerprint);
        }
    }
    void coalescedStatusesRemainSeparate() {
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out << quint32(DaemonMessages::FingerprintActive) << true << quint32(DaemonMessages::LoginSucceeded);
        QDataStream in(packet);
        LoginReply a, b;
        QVERIFY(readLoginReply(in, a));
        QVERIFY(readLoginReply(in, b));
        QCOMPARE(a.kind, DaemonMessages::FingerprintActive);
        QVERIFY(a.active);
        QCOMPARE(b.kind, DaemonMessages::LoginSucceeded);
    }
};
QTEST_GUILESS_MAIN(LoginMessagesTest)
#include "LoginMessagesTest.moc"
