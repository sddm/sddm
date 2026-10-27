// SPDX-License-Identifier: GPL-2.0-or-later
// Real upstream helper at the legacy path; actual current helper embedded.
#include "Auth.h"
#include "PeerCredentials.h"
#include <QFile>
#include <QSignalSpy>
#include <QtTest>
using namespace SDDM;
class ActualPairTest : public QObject {
    Q_OBJECT
    void trial(bool parallel, bool approve = false) {
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        const QString proof = fixture + QStringLiteral("/session-opened");
        const QString invoked = fixture + QStringLiteral("/old-invoked");
        QFile::remove(proof); QFile::remove(invoked);
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        auth.setPasswordOnly(parallel);
        auth.insertEnvironment(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
        auth.insertEnvironment(QStringLiteral("XDG_SESSION_TYPE"), QStringLiteral("wayland"));
        auth.insertEnvironment(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
        QSignalSpy result(&auth, &Auth::authentication), finished(&auth, &Auth::finished);
        if (parallel) QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        if (parallel && !approve)
            connect(&auth, &Auth::authentication, &auth, [&auth](const QString &, bool) { auth.setSessionApproved(false); });
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 10000);
        QVERIFY(!finished.isEmpty());
        QCOMPARE(result.size(), 1);
        QVERIFY(result.first().at(1).toBool());
        if (parallel) {
            QCOMPARE(QFile::exists(proof), approve);
            QVERIFY(!QFile::exists(invoked));
        } else {
            QVERIFY2(QFile::exists(proof), "legacy PAM session was not opened");
        }
    }
  private slots:
    void oldLegacyLogin() { trial(false); }
    void oldOrWrapperIsNotUsedForParallel() { trial(true); }
    void approvedParallelUsesCurrentEmbeddedHelper() { trial(true, true); }
    void currentHelperPamDenialIsNotACrash_data() {
        QTest::addColumn<bool>("fingerprint");
        QTest::newRow("password") << false;
        QTest::newRow("fingerprint") << true;
    }
    void currentHelperPamDenialIsNotACrash() {
        QFETCH(bool, fingerprint);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        const QString proof = qEnvironmentVariable("PAIR_FIXTURE") + QStringLiteral("/session-opened");
        QFile::remove(proof);
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        auth.setFingerprint(fingerprint);
        auth.setPasswordOnly(!fingerprint);
        QSignalSpy results(&auth, &Auth::authentication), errors(&auth, &Auth::error), finished(&auth, &Auth::finished);
        auth.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().at(1).toBool(), false);
        for (const auto &error : errors)
            QVERIFY(error.at(1).value<Auth::Error>() != Auth::ERROR_INTERNAL);
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_AUTH_ERROR);
        QVERIFY(!QFile::exists(proof));
        QVERIFY(parallelRuntimeAvailable());
    }
    void failedLegacyAllowsParallel() {
        QVERIFY(!parallelRuntimeAvailable());
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        QSignalSpy result(&auth, &Auth::authentication);
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 5000);
        if (!result.isEmpty()) {
            QVERIFY(!result.first().at(1).toBool());
            QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
            qInfo() << "legacy denial observed and capability restored";
        } else {
            qInfo() << "legacy denial unobserved; conservative gate retained";
            // If the old helper exits before its result is observed, keep the
            // conservative gate closed rather than infer a failed login.
            QVERIFY(!parallelRuntimeAvailable());
        }
    }
    void truncatedLegacyPeerDoesNotSeeEof() {
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(fixture + QStringLiteral("/peer-child")), 4000);
        QTest::qWait(qEnvironmentVariable("PAIR_MODE") == QStringLiteral("partial") ? 5500 : 400);
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/peer-saw-eof")));
        auth.cancelAuthentication();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 4000);
        QTest::qWait(200);
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/peer-saw-eof")));
    }
    void cancelOnLegacySuccessDoesNotOpenSession() {
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        QFile::remove(fixture + QStringLiteral("/session-opened"));
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        connect(&auth, &Auth::authentication, &auth, [&auth](const QString &, bool success) {
            if (success) auth.cancelAuthentication();
        });
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 5000);
        QTest::qWait(200);
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/session-opened")));
    }
    void cancelledLegacyCanRetryWithoutAcceptingOldChild() {
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        Auth auth;
        auth.setUser(QStringLiteral("old"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        auth.start();
        QTest::qWait(100);
        auth.cancelAuthentication();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 4000);
        auth.setUser(QStringLiteral("root"));
        QSignalSpy result(&auth, &Auth::authentication);
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 5000);
        QCOMPARE(result.size(), 1);
        QVERIFY(result.first().at(1).toBool());
        QVERIFY(QFile::exists(fixture + QStringLiteral("/session-root")));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(fixture + QStringLiteral("/old-child-started")), 3000);
        QTest::qWait(700);
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/session-old")));
    }
    void destroyedOrCancelledLegacyNeverEnablesParallel() {
        QVERIFY(!parallelRuntimeAvailable());
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        QFile::remove(fixture + QStringLiteral("/session-opened"));
        auto *auth = new Auth;
        auth->setUser(QStringLiteral("root"));
        auth->setSession(QStringLiteral("/usr/bin/true"));
        auth->start();
        if (qEnvironmentVariable("PAIR_MODE") == QStringLiteral("after"))
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(fixture + QStringLiteral("/pam-entered")), 4000);
        else
            QTest::qWait(100);
        if (qEnvironmentVariable("PAIR_ACTION") == QStringLiteral("cancel")) auth->cancelAuthentication();
        delete auth;
        if (qEnvironmentVariable("PAIR_MODE") == QStringLiteral("after")) {
            QFile resume(fixture + QStringLiteral("/continue"));
            QVERIFY(resume.open(QIODevice::WriteOnly));
        }
        QVERIFY(!parallelRuntimeAvailable());
        QTest::qWait(1200);
        const QString marker = qEnvironmentVariable("PAIR_MODE").startsWith(QStringLiteral("before")) ?
            QStringLiteral("/descendant-started") : QStringLiteral("/pam-entered");
        QVERIFY(QFile::exists(fixture + marker));
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/session-opened")));
        QVERIFY(!parallelRuntimeAvailable());
    }
    void detachedLegacyNeverEnablesParallel() {
        QVERIFY(!parallelRuntimeAvailable());
        QFile::remove(qEnvironmentVariable("PAIR_FIXTURE") + QStringLiteral("/session-opened")); // Start probe before the legacy child.
        Auth auth;
        auth.setUser(QStringLiteral("root"));
        auth.setSession(QStringLiteral("/usr/bin/true"));
        auth.start();
        QTRY_VERIFY_WITH_TIMEOUT(!auth.isActive(), 5000);
        const QString fixture = qEnvironmentVariable("PAIR_FIXTURE");
        if (qEnvironmentVariable("PAIR_MODE") == QStringLiteral("after")) {
            QFile resume(fixture + QStringLiteral("/continue"));
            QVERIFY(resume.open(QIODevice::WriteOnly));
        }
        QVERIFY(!parallelRuntimeAvailable());
        QTest::qWait(1200); // A delayed detached descendant may still connect.
        const QString marker = qEnvironmentVariable("PAIR_MODE").startsWith(QStringLiteral("before")) ?
            QStringLiteral("/descendant-started") : QStringLiteral("/pam-entered");
        QVERIFY2(QFile::exists(fixture + marker), "detached helper did not reach the expected boundary");
        QVERIFY(!QFile::exists(fixture + QStringLiteral("/session-opened")));
        QVERIFY(!parallelRuntimeAvailable());
    }
};
QTEST_GUILESS_MAIN(ActualPairTest)
#include "ActualPairTest.moc"
