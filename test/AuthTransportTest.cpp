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
};

QTEST_GUILESS_MAIN(AuthTransportTest)
#include "AuthTransportTest.moc"
