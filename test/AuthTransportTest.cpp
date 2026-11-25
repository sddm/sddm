#include <QtTest/QtTest>

#include "Auth.h"

using namespace SDDM;

class AuthTransportTest : public QObject {
    Q_OBJECT
private slots:
    void handoff_data() {
        QTest::addColumn<QString>("user");
        QTest::addColumn<AuthMethod>("method");
        QTest::newRow("hello-and-result-one-write") << QStringLiteral("coalesced") << AuthMethod::Password;
        QTest::newRow("hello-then-result") << QStringLiteral("split") << AuthMethod::Fingerprint;
        QTest::newRow("reject-foreign-peer-then-accept-child") << QStringLiteral("forged") << AuthMethod::Fingerprint;
        QTest::newRow("partial-hello-then-result") << QStringLiteral("fragmented") << AuthMethod::Password;
    }

    void handoff() {
        QFETCH(QString, user);
        QFETCH(AuthMethod, method);
        Auth auth;
        auth.setUser(user);
        auth.setMethod(method);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 2000);
        QCOMPARE(authenticated.at(0).at(0).toString(), user);
        QVERIFY(authenticated.at(0).at(1).toBool());
        auth.authorizeSession(false);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void cleanExitAfterAuthorization() {
        Auth auth;
        auth.setUser(QStringLiteral("clean-exit"));
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 2000);
        auth.authorizeSession(true);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.at(0).at(0).toInt(), int(Auth::HELPER_SUCCESS));
    }

    void rejectsStaleHelperFrame() {
        Auth auth;
        auth.setUser(QStringLiteral("wrong"));
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(authenticated.count(), 0);
    }

    void oldHandshakeTimerCannotKillRetry() {
        Auth auth;
        auth.setUser(QStringLiteral("slow"));
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 2000);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        const quint64 previous = auth.launchGeneration();
        QTest::qWait(4500);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 2, 1000);
        QVERIFY(auth.launchGeneration() > previous);
        QTest::qWait(650); // cross the original child's five-second timeout
        QVERIFY(auth.isActive());
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 2000);
    }

    void retryDoesNotAuthenticateOrLeakConversation() {
        Auth auth;
        auth.setUser(QStringLiteral("progress-retry-success"));
        auth.setMethod(AuthMethod::Fingerprint);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy information(&auth, &Auth::info);
        QSignalSpy error(&auth, &Auth::error);
        QSignalSpy finished(&auth, &Auth::finished);
        int progress = 0;
        connect(&auth, &Auth::authenticationMethodState, this, [&] { ++progress; });
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(progress, 2, 1000);
        QCOMPARE(authenticated.count(), 0);
        QCOMPARE(information.count(), 0);
        QCOMPARE(error.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 1000);
        QVERIFY(authenticated.at(0).at(1).toBool());
        QCOMPARE(progress, 2); // duplicate retry is not a fresh activation
        auth.authorizeSession(false);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void incompleteProgressCannotChangeConsumer() {
        Auth auth;
        auth.setUser(QStringLiteral("progress-fragmented"));
        auth.setMethod(AuthMethod::Fingerprint);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        int progress = 0;
        connect(&auth, &Auth::authenticationMethodState, this, [&] { ++progress; });
        auth.start();
        QTest::qWait(100);
        QCOMPARE(progress, 0);
        QCOMPARE(authenticated.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(progress, 1, 1000);
        QCOMPARE(authenticated.count(), 0);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void unavailablePrecedesFailureAndSurvivesExit() {
        Auth auth;
        auth.setUser(QStringLiteral("progress-unavailable"));
        auth.setMethod(AuthMethod::Fingerprint);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        QString terminalCause;
        QStringList events;
        connect(&auth, &Auth::authenticationMethodState, this,
                [&](const QString &, AuthMethod, FingerprintState state, const QString &detail) {
            if (state == FingerprintState::Unavailable) {
                terminalCause = detail;
                events << QStringLiteral("unavailable");
            }
        });
        connect(&auth, &Auth::authentication, this, [&](const QString &, bool success) {
            if (!success)
                events << QStringLiteral("failed");
        });
        connect(&auth, &Auth::finished, this, [&] { events << QStringLiteral("exited"); });
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(authenticated.count(), 1);
        QVERIFY(!authenticated.at(0).at(1).toBool());
        QCOMPARE(terminalCause, QStringLiteral("authentication_unavailable"));
        QCOMPARE(events, QStringList({QStringLiteral("unavailable"), QStringLiteral("failed"), QStringLiteral("exited")}));
    }

    void terminalFailureCannotLaterSelectWinner() {
        Auth auth;
        auth.setUser(QStringLiteral("progress-terminal-success"));
        auth.setMethod(AuthMethod::Fingerprint);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        int progress = 0;
        connect(&auth, &Auth::authenticationMethodState, this, [&] { ++progress; });
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(progress, 2);
        QCOMPARE(authenticated.count(), 0);
    }

    void rejectedProgressCannotReachSuccess_data() {
        QTest::addColumn<QString>("user");
        QTest::addColumn<AuthMethod>("method");
        for (const char *suffix : {"ready", "matched", "cancelled", "timeout", "unknown", "prose",
                                   "empty-code", "wrong-code", "wrong-user", "wrong-request", "wrong-method"})
            QTest::newRow(suffix) << QStringLiteral("progress-") + QLatin1String(suffix) << AuthMethod::Fingerprint;
        QTest::newRow("password-helper-claims-progress") << QStringLiteral("progress-password") << AuthMethod::Password;
        QTest::newRow("success-changes-bound-user") << QStringLiteral("wrong-authenticated-user") << AuthMethod::Fingerprint;
    }

    void rejectedProgressCannotReachSuccess() {
        QFETCH(QString, user);
        QFETCH(AuthMethod, method);
        Auth auth;
        auth.setUser(user);
        auth.setMethod(method);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        int progress = 0;
        connect(&auth, &Auth::authenticationMethodState, this, [&] { ++progress; });
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(authenticated.count(), 0);
        QCOMPARE(progress, 0);
    }

    void passwordConversationStillReachesConsumer() {
        Auth auth;
        auth.setUser(QStringLiteral("password-feedback"));
        QSignalSpy information(&auth, &Auth::info);
        QSignalSpy error(&auth, &Auth::error);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(information.count(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(error.count(), 1, 1000);
        QCOMPARE(information.at(0).at(0).toString(), QStringLiteral("stock password information"));
        QCOMPARE(error.at(0).at(0).toString(), QStringLiteral("stock password advisory"));
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void stopIsAsyncAndRejectsLateAuthentication() {
        Auth auth;
        auth.setUser(QStringLiteral("stop-late-success"));
        QSignalSpy information(&auth, &Auth::info);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(information.count(), 1, 1000);
        QElapsedTimer elapsed;
        elapsed.start();
        auth.stop();
        QVERIFY(elapsed.elapsed() < 250);
        QTest::qWait(350);
        QCOMPARE(authenticated.count(), 0);
        QVERIFY(auth.isActive());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 6000);
        QCOMPARE(authenticated.count(), 0);
    }

    void helperSocketPermissionsAndHandshakeFlood() {
        Auth auth;
        auth.setUser(QStringLiteral("socket-audit-flood"));
        auth.setMethod(AuthMethod::Fingerprint);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 4000);
        QCOMPARE(authenticated.at(0).at(0).toString(), QStringLiteral("socket-audit-flood"));
        QVERIFY(authenticated.at(0).at(1).toBool());
        auth.authorizeSession(false);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void sameChildCannotReconnectAfterHello() {
        Auth auth;
        auth.setUser(QStringLiteral("reconnect-after-hello"));
        auth.setMethod(AuthMethod::Password);
        auth.setDeferredAuthorization(true);
        QSignalSpy authenticated(&auth, &Auth::authentication);
        QSignalSpy finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.count(), 1, 3000);
        QCOMPARE(authenticated.at(0).at(0).toString(), QStringLiteral("reconnect-after-hello"));
        auth.authorizeSession(false);
        auth.kill();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
    }

    void postHelloMalformedFramesKillHelperWithoutAuthorizing() {
        for (int i = 0; i < 12; ++i) {
            Auth auth;
            auth.setUser(QStringLiteral("fuzz-post-hello-%1").arg(i));
            auth.setMethod(i % 2 == 0 ? AuthMethod::Fingerprint : AuthMethod::Password);
            auth.setDeferredAuthorization(true);
            QSignalSpy authenticated(&auth, &Auth::authentication);
            QSignalSpy finished(&auth, &Auth::finished);
            auth.start();
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
            QCOMPARE(authenticated.count(), 0);
        }
    }
};

QTEST_GUILESS_MAIN(AuthTransportTest)
#include "AuthTransportTest.moc"
