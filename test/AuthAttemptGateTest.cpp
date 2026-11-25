#include <QtTest/QtTest>

#include "AuthAttemptGate.h"

using namespace SDDM;

class AuthAttemptGateTest : public QObject {
    Q_OBJECT
private slots:
    void successOrder_data() {
        QTest::addColumn<AuthMethod>("winner");
        QTest::newRow("password-first") << AuthMethod::Password;
        QTest::newRow("fingerprint-first") << AuthMethod::Fingerprint;
    }

    void successOrder() {
        QFETCH(AuthMethod, winner);
        const AuthMethod loser = winner == AuthMethod::Password ? AuthMethod::Fingerprint : AuthMethod::Password;
        AuthAttemptGate gate;
        gate.begin(QStringLiteral("request-1"), QStringLiteral("alice"));
        QVERIFY(gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), winner));
        gate.waitFor(loser);
        QVERIFY(!gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), loser));
        QVERIFY(!gate.authorize(winner));
        gate.exited(loser);
        QVERIFY(gate.authorize(winner));
        QVERIFY(!gate.authorize(winner)); // simultaneous callbacks cannot start two sessions
        QVERIFY(!gate.authorize(loser));
    }

    void cancelledFingerprintCannotWinLate() {
        AuthAttemptGate gate;
        gate.begin(QStringLiteral("request-1"), QStringLiteral("alice"));
        gate.cancel(AuthMethod::Fingerprint);
        QVERIFY(!gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), AuthMethod::Fingerprint));
        QVERIFY(gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), AuthMethod::Password));
        gate.waitFor(AuthMethod::Fingerprint);
        QVERIFY(!gate.authorize(AuthMethod::Password));
        gate.exited(AuthMethod::Fingerprint);
        QVERIFY(gate.authorize(AuthMethod::Password));
    }

    void cancelledFingerprintExitedBeforePasswordWin() {
        AuthAttemptGate gate;
        gate.begin(QStringLiteral("request-1"), QStringLiteral("alice"));
        gate.cancel(AuthMethod::Fingerprint);
        gate.exited(AuthMethod::Fingerprint);
        QVERIFY(gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), AuthMethod::Password));
        QVERIFY(gate.authorize(AuthMethod::Password));
    }

    void rejectsStaleRequestUserAndInvalidatedAttempt() {
        AuthAttemptGate gate;
        gate.begin(QStringLiteral("old"), QStringLiteral("alice"));
        gate.invalidate(); // user/session change or display stop
        QVERIFY(!gate.select(QStringLiteral("old"), QStringLiteral("alice"), AuthMethod::Fingerprint));
        gate.begin(QStringLiteral("new"), QStringLiteral("bob"));
        QVERIFY(!gate.select(QStringLiteral("old"), QStringLiteral("bob"), AuthMethod::Password));
        QVERIFY(!gate.select(QStringLiteral("new"), QStringLiteral("alice"), AuthMethod::Password));
        QVERIFY(gate.select(QStringLiteral("new"), QStringLiteral("bob"), AuthMethod::Password));
        gate.invalidate();
        QVERIFY(!gate.authorize(AuthMethod::Password));
    }

    void fingerprintRetryAfterExit() {
        AuthAttemptGate gate;
        gate.begin(QStringLiteral("request-1"), QStringLiteral("alice"));
        gate.cancel(AuthMethod::Fingerprint);
        gate.exited(AuthMethod::Fingerprint);
        gate.resume(AuthMethod::Fingerprint);
        QVERIFY(gate.select(QStringLiteral("request-1"), QStringLiteral("alice"), AuthMethod::Fingerprint));
        QVERIFY(gate.authorize(AuthMethod::Fingerprint));
    }
};

QTEST_GUILESS_MAIN(AuthAttemptGateTest)
#include "AuthAttemptGateTest.moc"
