/*
 * Production GreeterProxy regressions using a private, deterministic daemon peer.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "Configuration.h"
#include "GreeterProxy.h"
#include "Messages.h"
#include "SessionModel.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace SDDM;

namespace {
    const QString idA = QStringLiteral("11111111-1111-4111-8111-111111111111");
    const QString idB = QStringLiteral("22222222-2222-4222-8222-222222222222");
    const QString idC = QStringLiteral("33333333-3333-4333-8333-333333333333");
    const QString desktopName = QStringLiteral("middle.desktop");
    QString desktop;

    template<typename Writer> QByteArray packet(Writer writer) {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        writer(stream);
        return bytes;
    }
    QByteArray capabilities(quint32 value) {
        return packet([value](QDataStream &s) { s << quint32(DaemonMessages::AuthenticationCapabilities) << value; });
    }
    QByteArray context(quint64 serial, const QString &id, const QString &user,
                       quint32 accepted = 1, quint32 phase = quint32(AuthenticationPhase::Idle),
                       const QString &detail = QStringLiteral("available"),
                       const QString &file = desktop,
                       AuthenticationAvailability availability = AuthenticationAvailability::Available) {
        return packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationContext) << serial << accepted << id << user
              << quint32(user.isEmpty() ? Session::UnknownSession : Session::X11Session)
              << (user.isEmpty() ? QString() : file)
              << quint32(user.isEmpty() ? AuthenticationAvailability::Unavailable : availability)
              << phase << detail;
        });
    }
    QByteArray state(quint64 serial, const QString &id, const QString &user, FingerprintState value,
                     AuthenticationMethod method = AuthenticationMethod::Fingerprint,
                     AuthenticationPhase phase = AuthenticationPhase::Active,
                     const QString &detail = QStringLiteral("scanning"), const QString &file = desktop) {
        return packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationMethodState) << serial << id << quint32(method) << user
              << quint32(Session::X11Session) << file << quint32(value) << quint32(phase) << detail;
        });
    }
    QByteArray finished(quint64 serial, const QString &id, const QString &user,
                        AuthenticationResult result, AuthenticationMethod winner) {
        return packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationAttemptFinished) << serial << id << user
              << quint32(Session::X11Session) << desktop << quint32(result) << quint32(winner);
        });
    }
    QByteArray legacy(DaemonMessages message) {
        return packet([&](QDataStream &s) { s << quint32(message); });
    }
    QByteArray information() {
        return packet([](QDataStream &s) { s << quint32(DaemonMessages::InformationMessage) << QStringLiteral("old feedback"); });
    }

    struct Request {
        quint32 message { 0 }, hasContext { 0 }, method { 0 }, type { 0 };
        quint64 serial { 0 };
        QString user, password, file, id;
    };
}

class GreeterProxyTest : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<SessionModel> m_model;
    std::unique_ptr<GreeterProxy> m_proxy;
    QLocalServer m_server;
    QLocalSocket *m_peer { nullptr };
    QVector<Request> m_requests;
    bool m_peerDecodeError { false };

    void send(const QByteArray &bytes) {
        m_peer->write(bytes);
        m_peer->flush();
    }
    bool writeDesktop(const QString &file) {
        QFile fixture(m_dir->filePath(file));
        return fixture.open(QIODevice::WriteOnly)
            && fixture.write("[Desktop Entry]\nName=Private proxy session\nExec=/bin/true\nTryExec=/bin/true\nType=Application\n") > 0;
    }
    void readRequests() {
        QDataStream stream(m_peer);
        while (m_peer->bytesAvailable() >= qint64(sizeof(quint32))) {
            Request request;
            stream.startTransaction();
            stream >> request.message;
            switch (GreeterMessages(request.message)) {
                case GreeterMessages::Connect:
                case GreeterMessages::QueryAuthenticationCapabilities:
                case GreeterMessages::PowerOff:
                case GreeterMessages::Reboot:
                case GreeterMessages::Suspend:
                case GreeterMessages::Hibernate:
                case GreeterMessages::HybridSleep: break;
                case GreeterMessages::SetAuthenticationContext:
                    stream >> request.serial >> request.hasContext;
                    if (request.hasContext) stream >> request.user >> request.type >> request.file;
                    break;
                case GreeterMessages::StartAuthenticationMethod: stream >> request.serial >> request.method; break;
                case GreeterMessages::CancelAuthenticationMethod: stream >> request.serial >> request.id >> request.method; break;
                case GreeterMessages::Login: stream >> request.user >> request.password >> request.type >> request.file; break;
                default: m_peerDecodeError = true; stream.abortTransaction(); return;
            }
            if (!stream.commitTransaction()) return;
            m_requests.append(request);
        }
    }

private slots:
    void initTestCase() {
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            m_peer = m_server.nextPendingConnection();
            connect(m_peer, &QLocalSocket::readyRead, this, &GreeterProxyTest::readRequests);
            readRequests();
        });
    }
    void init() {
        m_dir = std::make_unique<QTemporaryDir>(QDir::currentPath() + QStringLiteral("/greeter-proxy-XXXXXX"));
        QVERIFY(m_dir->isValid());
        QVERIFY(writeDesktop(desktopName));
        desktop = m_dir->filePath(desktopName);
        mainConfig.X11.SessionDir.set(QStringList{m_dir->path()});
        mainConfig.Wayland.SessionDir.set(QStringList{});
        m_model = std::make_unique<SessionModel>();
        QCOMPARE(m_model->rowCount(), 1);
        m_requests.clear();
        m_peerDecodeError = false;
        QVERIFY(m_server.listen(m_dir->filePath(QStringLiteral("peer"))));
        m_proxy = std::make_unique<GreeterProxy>(m_server.fullServerName());
        m_proxy->setSessionModel(m_model.get());
        m_proxy->setAuthenticationController(true);
        QTRY_VERIFY(m_peer != nullptr);
        QTRY_COMPARE(m_requests.size(), 2);
        QCOMPARE(m_requests.at(0).message, quint32(GreeterMessages::Connect));
        QCOMPARE(m_requests.at(1).message, quint32(GreeterMessages::QueryAuthenticationCapabilities));
        m_requests.clear();
    }
    void cleanup() {
        QVERIFY(!m_peerDecodeError);
        if (m_proxy) disconnect(m_proxy.get(), nullptr, this, nullptr);
        m_proxy.reset();
        delete m_peer;
        m_peer = nullptr;
        m_server.close();
        m_model.reset();
        m_dir.reset();
    }

    void incompleteFramesCannotPublish() {
        QSignalSpy methods(m_proxy.get(), &GreeterProxy::authenticationMethodsAvailableChanged);
        const auto caps = capabilities(2);
        send(caps.left(6));
        QTest::qWait(10);
        QVERIFY(!m_proxy->authenticationMethodsAvailable());
        QCOMPARE(methods.size(), 0);
        send(caps.mid(6));
        QTRY_COMPARE(methods.size(), 1);
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        QSignalSpy ids(m_proxy.get(), &GreeterProxy::fingerprintRequestIdChanged);
        const auto ack = context(serial, idA, QStringLiteral("alice"));
        send(ack.left(ack.size() - 2));
        QTest::qWait(10);
        QCOMPARE(ids.size(), 0);
        QCOMPARE(m_proxy->fingerprintRequestId(), QString());
        QVERIFY(!m_proxy->fingerprintAvailable());
        send(ack.right(2));
        QTRY_COMPARE(ids.size(), 1);
        QSignalSpy states(m_proxy.get(), &GreeterProxy::fingerprintStateChanged);
        const auto scan = state(serial, idA, QStringLiteral("alice"), FingerprintState::Scanning);
        send(scan.left(scan.size() - 2));
        QTest::qWait(10);
        QCOMPARE(states.size(), 0);
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("ready"));
        send(scan.right(2));
        QTRY_COMPARE(states.size(), 1);
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        const auto done = finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Password);
        send(done.left(done.size() - 1));
        QTest::qWait(10);
        QCOMPARE(success.size(), 0);
        QVERIFY(!m_proxy->authenticationBusy());
        send(done.right(1));
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(m_proxy->authenticationBusy());
    }

    void rapidReplacementAndSnapshotCoherence() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto a = m_requests.last().serial;
        send(context(a, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        send(state(a, idA, QStringLiteral("alice"), FingerprintState::Scanning));
        QTRY_COMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        bool coherent = true;
        connect(m_proxy.get(), &GreeterProxy::fingerprintRequestIdChanged, this, [&](const QString &id) {
            if (!id.isEmpty()) coherent = coherent && m_proxy->authenticationUser() == QLatin1String("alice")
                && m_proxy->authenticationSessionIndex() == 0 && m_proxy->fingerprintAvailable()
                && m_proxy->fingerprintState() == QLatin1String("ready");
        });
        m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 3);
        const auto b = m_requests.at(1).serial;
        const auto c = m_requests.at(2).serial;
        QVERIFY(a < b && b < c);
        send(context(b, idB, QStringLiteral("bob")) + state(a, idA, QStringLiteral("alice"), FingerprintState::Matched)
             + context(c, idB, QStringLiteral("bob")) + context(c, idC, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idC);
        QVERIFY(coherent);
        send(context(c, idB, QStringLiteral("alice"))
             + state(c, idC, QStringLiteral("alice"), FingerprintState::Matched, AuthenticationMethod::Fingerprint,
                     AuthenticationPhase::SessionAuthorized, QStringLiteral("matched"), QStringLiteral("other.desktop"))
             + finished(a, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Fingerprint));
        QTest::qWait(10);
        QCOMPARE(m_proxy->fingerprintRequestId(), idC);
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("ready"));
        QVERIFY(!m_proxy->authenticationBusy());
    }

    void cancellationBeforeAndAfterAcknowledgement() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        m_proxy->cancelFingerprint();
        QTRY_COMPARE(m_requests.size(), 3);
        const auto serial = m_requests.at(0).serial;
        QCOMPARE(m_requests.at(1).message, quint32(GreeterMessages::StartAuthenticationMethod));
        QCOMPARE(m_requests.at(1).serial, serial);
        QCOMPARE(m_requests.at(2).message, quint32(GreeterMessages::CancelAuthenticationMethod));
        QCOMPARE(m_requests.at(2).serial, serial);
        QVERIFY(m_requests.at(2).id.isEmpty());
        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        m_proxy->cancelFingerprint();
        QTRY_COMPARE(m_requests.size(), 4);
        QCOMPARE(m_requests.last().id, idA);
        m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        m_proxy->cancelFingerprint();
        QTRY_COMPARE(m_requests.size(), 6);
        QVERIFY(m_requests.last().serial > serial);
        QVERIFY(m_requests.last().id.isEmpty());
    }

    void passwordFailureDoesNotEndScanAndPasswordWinnerLocks() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice"))
             + state(serial, idA, QStringLiteral("alice"), FingerprintState::Scanning));
        QTRY_COMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        QSignalSpy failed(m_proxy.get(), &GreeterProxy::loginFailed);
        QSignalSpy info(m_proxy.get(), &GreeterProxy::informationMessage);
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("private test response"), 0);
        QTRY_COMPARE(m_requests.size(), 2);
        send(state(serial, idA, QStringLiteral("alice"), FingerprintState::Failed, AuthenticationMethod::Password,
                   AuthenticationPhase::Active, QStringLiteral("authentication_failed"))
             + legacy(DaemonMessages::LoginFailed) + information());
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(info.size(), 0);
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        QVERIFY(m_proxy->authenticationSubmissionAllowed());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("retry response"), 0);
        QTRY_COMPARE(m_requests.size(), 3);
        send(state(serial, idA, QStringLiteral("alice"), FingerprintState::Matched, AuthenticationMethod::Password,
                   AuthenticationPhase::WinnerSelected, QStringLiteral("matched")));
        QTRY_VERIFY(m_proxy->authenticationBusy());
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        m_proxy->login(QStringLiteral("bob"), QStringLiteral("replacement"), 0);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 3);
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Password)
             + legacy(DaemonMessages::LoginSucceeded)
             + finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Password));
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(m_proxy->authenticationBusy());
    }

    void unavailableExplicitRetryConsumesOnlyFreshCurrentAvailability() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice"), 1, quint32(AuthenticationPhase::Idle),
                     QStringLiteral("service_unavailable"), desktop, AuthenticationAvailability::Unavailable)
             + state(serial, idA, QStringLiteral("alice"), FingerprintState::Unavailable,
                     AuthenticationMethod::Fingerprint, AuthenticationPhase::Idle, QStringLiteral("service_unavailable")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("unavailable"));
        QVERIFY(!m_proxy->fingerprintAvailable());
        QVERIFY(m_proxy->authenticationSubmissionAllowed());
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 1);

        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 2);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::StartAuthenticationMethod));
        QCOMPARE(m_requests.last().serial, serial);
        QCOMPARE(m_requests.last().method, quint32(AuthenticationMethod::Fingerprint));
        QCOMPARE(m_proxy->fingerprintRequestId(), idA);
        QVERIFY(!m_proxy->fingerprintAvailable());
        send(context(serial + 1, idA, QStringLiteral("alice"))
             + context(serial, idB, QStringLiteral("alice"))
             + context(serial, idA, QStringLiteral("bob"))
             + context(serial, idA, QStringLiteral("alice"), 1, quint32(AuthenticationPhase::Idle),
                       QStringLiteral("available"), QStringLiteral("other.desktop")));
        QTest::qWait(10);
        QVERIFY(!m_proxy->fingerprintAvailable());
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("unavailable"));
        QCOMPARE(m_requests.size(), 2);

        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_VERIFY(m_proxy->fingerprintAvailable());
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("ready"));
        QCOMPARE(m_proxy->fingerprintDetailCode(), QStringLiteral("available"));
        QCOMPARE(m_proxy->authenticationUser(), QStringLiteral("alice"));
        QCOMPARE(m_proxy->authenticationSessionIndex(), 0);
        QCOMPARE(m_proxy->fingerprintRequestId(), idA);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 2);
        send(state(serial, idA, QStringLiteral("alice"), FingerprintState::Scanning));
        QTRY_COMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        QVERIFY(m_proxy->authenticationSubmissionAllowed());
        send(state(serial, idA, QStringLiteral("alice"), FingerprintState::Unavailable,
                   AuthenticationMethod::Fingerprint, AuthenticationPhase::Idle, QStringLiteral("authentication_unavailable")));
        QTRY_COMPARE(m_proxy->fingerprintState(), QStringLiteral("unavailable"));
        QVERIFY(!m_proxy->fingerprintAvailable());
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 2);
    }

    void completionBeforeCommittedRejectionRestoresOnlyExactTuple_data() {
        QTest::addColumn<bool>("clear");
        QTest::newRow("finished-selection") << false;
        QTest::newRow("finished-clear") << true;
    }

    void completionBeforeCommittedRejectionRestoresOnlyExactTuple() {
        QFETCH(bool, clear);
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice"))
             + state(serial, idA, QStringLiteral("alice"), FingerprintState::Scanning));
        QTRY_COMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        if (clear) m_proxy->clearAuthenticationContext();
        else m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        QTRY_COMPARE(m_requests.size(), 2);
        const auto replacement = m_requests.last().serial;
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        QSignalSpy rejected(m_proxy.get(), &GreeterProxy::authenticationContextRejected);
        QSignalSpy completion(m_proxy.get(), &GreeterProxy::authenticationFinished);
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Fingerprint)
             + context(replacement, idB, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed"))
             + context(replacement + 1, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed"))
             + context(replacement, idA, QStringLiteral("bob"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed"))
             + context(replacement, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed"), QStringLiteral("other.desktop"))
             + context(replacement, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("context_busy"))
             + context(replacement, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Active), QStringLiteral("session_committed")));
        QTest::qWait(10);
        QCOMPARE(success.size(), 0);
        QCOMPARE(rejected.size(), 0);
        QCOMPARE(m_proxy->authenticationUser(), clear ? QString() : QStringLiteral("bob"));
        send(context(replacement, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed")));
        QTRY_COMPARE(success.size(), 1);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(completion.size(), 1);
        QCOMPARE(m_proxy->authenticationUser(), QStringLiteral("alice"));
        QCOMPARE(m_proxy->authenticationSessionIndex(), 0);
        QCOMPARE(m_proxy->fingerprintRequestId(), idA);
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("scanning"));
        QVERIFY(m_proxy->authenticationBusy());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        m_proxy->login(QStringLiteral("bob"), QStringLiteral("blocked"), 0);
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Fingerprint)
             + context(replacement, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed")));
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 2);
        QCOMPARE(success.size(), 1);
        QCOMPARE(completion.size(), 1);
        QCOMPARE(rejected.size(), 1);
    }

    void acceptedReplacementDiscardsRetainedCompletion() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto a = m_requests.last().serial;
        send(context(a, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        QTRY_COMPARE(m_requests.size(), 2);
        const auto b = m_requests.last().serial;
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        send(finished(a, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded, AuthenticationMethod::Password)
             + context(b, idB, QStringLiteral("bob"))
             + context(b, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Finished), QStringLiteral("session_committed")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idB);
        QCOMPARE(success.size(), 0);
        QCOMPARE(m_proxy->authenticationUser(), QStringLiteral("bob"));
    }

    void committedFallbackFailureRequiresFreshAcceptedBarrier() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        m_proxy->disableAuthenticationMethods();
        QTRY_COMPARE(m_requests.size(), 2);
        const auto firstClear = m_requests.last().serial;
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("blocked"), 0);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 2);
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Failed, AuthenticationMethod::Fingerprint)
             + context(firstClear, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::SessionAuthorized), QStringLiteral("session_committed")));
        QTRY_COMPARE(m_requests.size(), 3);
        QCOMPARE(m_requests.last().hasContext, quint32(0));
        const auto secondClear = m_requests.last().serial;
        QVERIFY(secondClear > firstClear);
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        QSignalSpy cleared(m_proxy.get(), &GreeterProxy::authenticationContextCleared);
        QSignalSpy failed(m_proxy.get(), &GreeterProxy::loginFailed);
        QSignalSpy info(m_proxy.get(), &GreeterProxy::informationMessage);
        connect(m_proxy.get(), &GreeterProxy::authenticationContextCleared, this, [&](quint64) {
            m_proxy->login(QStringLiteral("alice"), QStringLiteral("new fallback response"), 0);
        });
        send(context(firstClear, QString(), QString(), 1, quint32(AuthenticationPhase::Idle), QStringLiteral("cleared")));
        QTest::qWait(10);
        QCOMPARE(cleared.size(), 0);
        send(context(secondClear, QString(), QString(), 1, quint32(AuthenticationPhase::Idle), QStringLiteral("cleared"))
             + legacy(DaemonMessages::LoginFailed) + information());
        QTRY_COMPARE(cleared.size(), 1);
        QTRY_COMPARE(m_requests.size(), 4);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::Login));
        QCOMPARE(failed.size(), 0);
        QCOMPARE(info.size(), 0);
        QVERIFY(m_proxy->authenticationSubmissionAllowed());
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        send(legacy(DaemonMessages::LoginSucceeded));
        QTRY_COMPARE(success.size(), 1);
        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 4);
    }

    void controllerPromotionCannotSubmitBeforeClear() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        m_proxy->setAuthenticationController(false);
        m_proxy->clearAuthenticationContext();
        m_proxy->setAuthenticationController(true);
        QTRY_COMPARE(m_requests.size(), 2);
        m_proxy->startFingerprint(QStringLiteral("bob"), 0);
        m_proxy->login(QStringLiteral("bob"), QStringLiteral("blocked"), 0);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 2);
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        send(context(m_requests.last().serial, QString(), QString(), 1, quint32(AuthenticationPhase::Idle), QStringLiteral("cleared")));
        QTRY_VERIFY(m_proxy->authenticationSubmissionAllowed());
        m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
        QTRY_COMPARE(m_requests.size(), 3);
        QCOMPARE(m_requests.last().user, QStringLiteral("bob"));
    }

    void accountFailureAllowsExplicitFreshAttempt() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice"))
             + state(serial, idA, QStringLiteral("alice"), FingerprintState::Matched, AuthenticationMethod::Fingerprint,
                     AuthenticationPhase::SessionAuthorized, QStringLiteral("matched")));
        QTRY_VERIFY(m_proxy->authenticationBusy());
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Failed, AuthenticationMethod::Fingerprint));
        QTRY_VERIFY(!m_proxy->authenticationBusy());
        QCOMPARE(m_proxy->fingerprintState(), QStringLiteral("failed"));
        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 3);
        QVERIFY(m_requests.at(1).serial > serial);
        QCOMPARE(m_requests.at(2).serial, m_requests.at(1).serial);
        QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
    }

    void stableSessionIdentitySurvivesResetAndRemovalClears() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        QVERIFY(writeDesktop(QStringLiteral("first.desktop")));
        QTRY_COMPARE(m_model->rowCount(), 2);
        QTRY_COMPARE(m_proxy->authenticationSessionIndex(), 1);
        QCOMPARE(m_proxy->fingerprintRequestId(), idA);
        QCOMPARE(m_requests.size(), 1);
        QCOMPARE(m_model->sessionIdentity(1).value(QStringLiteral("file")).toString(), desktop);
        QVERIFY(QFile::remove(desktop));
        QTRY_COMPARE(m_requests.size(), 2);
        QCOMPARE(m_requests.last().hasContext, quint32(0));
        QCOMPARE(m_proxy->authenticationSessionIndex(), -1);
        QVERIFY(m_proxy->authenticationUser().isEmpty());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
    }

    void stockDaemonPasswordAndSprintOneRefusal() {
        // No capability reply is the stock-daemon case; it never delays Login.
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("stock response"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::Login));
        QSignalSpy failed(m_proxy.get(), &GreeterProxy::loginFailed);
        send(legacy(DaemonMessages::LoginFailed));
        QTRY_COMPARE(failed.size(), 1);
        send(capabilities(1));
        QTRY_COMPARE(m_proxy->fingerprintDetailCode(), QStringLiteral("daemon_upgrade_required"));
        QVERIFY(!m_proxy->authenticationMethodsAvailable());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("refused"), 0);
        m_proxy->startFingerprint(QStringLiteral("alice"), 0);
        QTest::qWait(10);
        QCOMPARE(m_requests.size(), 1);
    }

    void stockThemeOnUpdatedDaemonDoesNotOptIn() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("stock-theme response"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::Login));
        QCOMPARE(m_proxy->authenticationUser(), QStringLiteral("alice"));
        QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
        QSignalSpy failed(m_proxy.get(), &GreeterProxy::loginFailed);
        send(legacy(DaemonMessages::LoginFailed));
        QTRY_COMPARE(failed.size(), 1);
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 2);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::SetAuthenticationContext));
    }

    void nonOptedCommittedClearRetainsStockCompletionWithoutUuidAdoption_data() {
        QTest::addColumn<bool>("succeeded");
        QTest::newRow("authorized-failure") << false;
        QTest::newRow("finished-success") << true;
    }

    void nonOptedCommittedClearRetainsStockCompletionWithoutUuidAdoption() {
        QFETCH(bool, succeeded);
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("stock committed response"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        m_proxy->disableAuthenticationMethods();
        QTRY_COMPARE(m_requests.size(), 2);
        const auto clear = m_requests.last().serial;
        QSignalSpy failed(m_proxy.get(), &GreeterProxy::loginFailed);
        QSignalSpy correlatedFinished(m_proxy.get(), &GreeterProxy::authenticationFinished);
        QSignalSpy success(m_proxy.get(), &GreeterProxy::loginSucceeded);
        QSignalSpy rejected(m_proxy.get(), &GreeterProxy::authenticationContextRejected);
        const auto result = succeeded ? DaemonMessages::LoginSucceeded : DaemonMessages::LoginFailed;
        const auto phase = quint32(succeeded ? AuthenticationPhase::Finished : AuthenticationPhase::SessionAuthorized);
        send(legacy(result)
             + context(clear + 1, idA, QStringLiteral("alice"), 0, phase, QStringLiteral("session_committed"))
             + context(clear, idA, QStringLiteral("bob"), 0, phase, QStringLiteral("session_committed"))
             + context(clear, idA, QStringLiteral("alice"), 0, phase, QStringLiteral("session_committed"), QStringLiteral("other.desktop"))
             + context(clear, idA, QStringLiteral("alice"), 0, phase, QStringLiteral("context_busy"))
             + context(clear, idA, QStringLiteral("alice"), 0, quint32(AuthenticationPhase::Active), QStringLiteral("session_committed")));
        QTest::qWait(10);
        QCOMPARE(success.size(), 0);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(rejected.size(), 0);
        QVERIFY(m_proxy->authenticationUser().isEmpty());
        send(context(clear, idA, QStringLiteral("alice"), 0, phase, QStringLiteral("session_committed")));
        QTRY_COMPARE(rejected.size(), 1);
        QCOMPARE(success.size(), succeeded ? 1 : 0);
        QCOMPARE(failed.size(), succeeded ? 0 : 1);
        QCOMPARE(correlatedFinished.size(), 0);
        QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        if (succeeded) {
            QCOMPARE(m_proxy->authenticationUser(), QStringLiteral("alice"));
            QCOMPARE(m_proxy->authenticationSessionIndex(), 0);
            QVERIFY(m_proxy->authenticationBusy());
            m_proxy->login(QStringLiteral("bob"), QStringLiteral("blocked"), 0);
            m_proxy->setAuthenticationContext(QStringLiteral("bob"), 0);
            m_proxy->startFingerprint(QStringLiteral("alice"), 0);
            send(legacy(result) + context(clear, idB, QStringLiteral("alice"), 0, phase, QStringLiteral("session_committed")));
            QTest::qWait(10);
            QCOMPARE(m_requests.size(), 2);
            QCOMPARE(success.size(), 1);
            QCOMPARE(rejected.size(), 1);
            QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
            return;
        }
        QTRY_COMPARE(m_requests.size(), 3);
        const auto freshClear = m_requests.last().serial;
        QVERIFY(freshClear > clear);
        QCOMPARE(correlatedFinished.size(), 0);
        QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
        send(context(freshClear, QString(), QString(), 1,
                     quint32(AuthenticationPhase::Idle), QStringLiteral("cleared")));
        QTRY_VERIFY(m_proxy->authenticationSubmissionAllowed());
        m_proxy->login(QStringLiteral("alice"), QStringLiteral("fallback after clear"), 0);
        QTRY_COMPARE(m_requests.size(), 4);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::Login));
    }

    void closedEnumRejectsWithoutPublishing() {
        send(capabilities(2));
        QTRY_VERIFY(m_proxy->authenticationMethodsAvailable());
        m_proxy->setAuthenticationContext(QStringLiteral("alice"), 0);
        QTRY_COMPARE(m_requests.size(), 1);
        const auto serial = m_requests.last().serial;
        send(context(serial, idA, QStringLiteral("alice")));
        QTRY_COMPARE(m_proxy->fingerprintRequestId(), idA);
        QSignalSpy completion(m_proxy.get(), &GreeterProxy::authenticationFinished);
        QSignalSpy disconnected(m_proxy.get(), &GreeterProxy::socketDisconnected);
        send(finished(serial, idA, QStringLiteral("alice"), AuthenticationResult::Succeeded,
                      static_cast<AuthenticationMethod>(3)));
        QTRY_COMPARE(disconnected.size(), 1);
        QCOMPARE(completion.size(), 0);
        QVERIFY(m_proxy->fingerprintRequestId().isEmpty());
        QVERIFY(!m_proxy->authenticationSubmissionAllowed());
    }
};

QTEST_GUILESS_MAIN(GreeterProxyTest)
#include "GreeterProxyTest.moc"
