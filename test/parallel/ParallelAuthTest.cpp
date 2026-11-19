// SPDX-License-Identifier: GPL-2.0-or-later

#include "Auth.h"
#include "AuthenticationSelection.h"
#include "Configuration.h"
#include "PeerCredentials.h"
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace SDDM;
class ParallelAuthTest : public QObject {
    Q_OBJECT
    static void setup(Auth &auth, const QString &proof, int delay, const QString &mode = QStringLiteral("ok")) {
        auth.setUser(QStringLiteral("fixture"));
        auth.setSession(QString::number(delay) + QLatin1Char(';') + mode + QLatin1Char(';') + proof);
    }
    static QByteArray contents(const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }
    void race(int passwordDelay, int fingerprintDelay, bool passwordFails = false) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Auth password, fingerprint;
        fingerprint.setFingerprint(true);
        password.setPasswordOnly(true);
        AuthenticationSelection selection;
        selection.selectUser(QStringLiteral("fixture"));
        int accepted = 0;
        for (Auth *auth : {&password, &fingerprint})
            connect(auth, &Auth::authentication, this, [&, auth](const QString &user, bool success) {
                if (!success)
                    return;
                bool winner = selection.accept(auth, user);
                auth->setSessionApproved(winner);
                if (winner) {
                    accepted++;
                    (auth == &password ? fingerprint : password).cancelAuthentication();
                }
            });
        setup(password, dir.filePath(QStringLiteral("password")), passwordDelay,
              passwordFails ? QStringLiteral("fail") : QStringLiteral("ok"));
        setup(fingerprint, dir.filePath(QStringLiteral("fingerprint")), fingerprintDelay);
        fingerprint.start();
        password.start();
        QVERIFY(password.isActive());
        QVERIFY(fingerprint.isActive());
        QTRY_COMPARE(accepted, 1);
        QTest::qWait(150);
        QCOMPARE(accepted, 1);
        const auto p = contents(dir.filePath(QStringLiteral("password"))),
                   f = contents(dir.filePath(QStringLiteral("fingerprint")));
        QCOMPARE(int(p == "approved") + int(f == "approved"), 1);
        if (passwordFails)
            QCOMPARE(f, QByteArray("approved"));
        password.stop();
        fingerprint.stop();
    }
    static void channel(Auth &auth, const QString &kind) {
        if (kind == QStringLiteral("fingerprint")) auth.setFingerprint(true);
        else if (kind == QStringLiteral("password")) auth.setPasswordOnly(true);
    }
    static void channels() {
        QTest::addColumn<QString>("kind");
        for (const QString &kind : {QStringLiteral("fingerprint"), QStringLiteral("password"), QStringLiteral("legacy")})
            QTest::newRow(qPrintable(kind)) << kind;
    }
  private slots:
    void init() {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
#endif
    }
    void passwordWinsWhileFingerprintIsActive() {
        race(150, 1000);
    }
    void fingerprintWinsWhilePasswordIsActive() {
        race(1000, 150);
    }
    void simultaneousMatchesOpenOnlyOneSession() {
        for (int i = 0; i < 10; i++)
            race(80, 80);
    }
    void failedPasswordLeavesFingerprintAvailable() {
        race(100, 700, true);
    }
    void reportedFailureDisconnectExitsNormally_data() { channels(); }
    void reportedFailureDisconnectExitsNormally() {
        QFETCH(QString, kind);
        QTemporaryDir dir;
        Auth a;
        channel(a, kind);
        setup(a, dir.filePath(QStringLiteral("proof")), 50, QStringLiteral("fail-disconnect"));
        QSignalSpy auth(&a, &Auth::authentication), errors(&a, &Auth::error), finished(&a, &Auth::finished);
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 4000);
        QCOMPARE(auth.size(), 1);
        QCOMPARE(auth.first().at(1).toBool(), false);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_AUTH_ERROR);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void realCrashAfterFailureStaysInternal_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        for (const QString &kind : {QStringLiteral("fingerprint"), QStringLiteral("password"), QStringLiteral("legacy")})
            for (const QString &mode : {QStringLiteral("fail-crash"), QStringLiteral("fail-disconnect-crash")})
                QTest::newRow(qPrintable(kind + mode)) << kind << mode;
    }
    void realCrashAfterFailureStaysInternal() {
        QFETCH(QString, kind);
        QFETCH(QString, mode);
        QTemporaryDir dir;
        Auth a;
        channel(a, kind);
        setup(a, dir.filePath(QStringLiteral("proof")), 30, mode);
        QSignalSpy auth(&a, &Auth::authentication), errors(&a, &Auth::error), finished(&a, &Auth::finished);
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QCOMPARE(auth.size(), 1);
        QCOMPARE(auth.first().at(1).toBool(), false);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().at(1).value<Auth::Error>(), Auth::ERROR_INTERNAL);
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_OTHER_ERROR);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void failedHelperCannotRemainAlive_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        for (const QString &kind : {QStringLiteral("fingerprint"), QStringLiteral("password"), QStringLiteral("legacy")})
            for (const QString &mode : {QStringLiteral("fail-hang"), QStringLiteral("fail-disconnect-hang")})
                QTest::newRow(qPrintable(kind + mode)) << kind << mode;
    }
    void failedHelperCannotRemainAlive() {
        QFETCH(QString, kind);
        QFETCH(QString, mode);
        QTemporaryDir dir;
        Auth a;
        channel(a, kind);
        setup(a, dir.filePath(QStringLiteral("proof")), 30, mode);
        QSignalSpy auth(&a, &Auth::authentication), errors(&a, &Auth::error), finished(&a, &Auth::finished);
        QElapsedTimer elapsed;
        elapsed.start();
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QVERIFY(elapsed.elapsed() < 2500);
        QCOMPARE(auth.size(), 1);
        QCOMPARE(auth.first().at(1).toBool(), false);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().first().toString().contains(QStringLiteral("did not finish")));
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_OTHER_ERROR);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void newAttemptHasNoFailedExitTimer_data() { channels(); }
    void newAttemptHasNoFailedExitTimer() {
        QFETCH(QString, kind);
        QTemporaryDir dir;
        Auth a;
        channel(a, kind);
        const QString proof = dir.filePath(QStringLiteral("proof"));
        setup(a, proof, 30, QStringLiteral("fail-disconnect"));
        QSignalSpy errors(&a, &Auth::error), finished(&a, &Auth::finished);
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        setup(a, proof, 1300);
        connect(&a, &Auth::authentication, &a, [&](const QString &, bool success) { a.setSessionApproved(success); });
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(contents(proof), QByteArray("approved"), 3000);
        QTest::qWait(1100);
        QVERIFY(a.isActive());
        QCOMPARE(errors.size(), 0);
        a.stop();
    }
    void failedPeerCannotSendAnotherResult_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        for (const QString &kind : {QStringLiteral("fingerprint"), QStringLiteral("password"), QStringLiteral("legacy")})
            for (const QString &mode : {QStringLiteral("fail-then-success"), QStringLiteral("fail-disconnect-partial"), QStringLiteral("fail-disconnect-success-exit")})
                QTest::newRow(qPrintable(kind + mode)) << kind << mode;
    }
    void failedPeerCannotSendAnotherResult() {
        QFETCH(QString, kind);
        QFETCH(QString, mode);
        QTemporaryDir dir;
        Auth a;
        channel(a, kind);
        const QString proof = dir.filePath(QStringLiteral("proof"));
        setup(a, proof, 30, mode);
        connect(&a, &Auth::authentication, &a, [&](const QString &, bool success) { a.setSessionApproved(success); });
        QSignalSpy auth(&a, &Auth::authentication), errors(&a, &Auth::error), finished(&a, &Auth::finished);
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QCOMPARE(auth.size(), 1);
        QCOMPARE(auth.first().at(1).toBool(), false);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().at(1).value<Auth::Error>(), Auth::ERROR_INTERNAL);
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_OTHER_ERROR);
        QVERIFY(!QFile::exists(proof));
    }
    void passwordCanWinDuringFailedFingerprintCleanup() {
        QTemporaryDir dir;
        Auth fingerprint, password;
        fingerprint.setFingerprint(true);
        password.setPasswordOnly(true);
        setup(fingerprint, dir.filePath(QStringLiteral("fingerprint")), 30, QStringLiteral("fail-disconnect"));
        setup(password, dir.filePath(QStringLiteral("password")), 50);
        QSignalSpy failed(&fingerprint, &Auth::authentication), errors(&fingerprint, &Auth::error);
        connect(&password, &Auth::authentication, &password, [&](const QString &, bool success) {
            password.setSessionApproved(success);
            if (success) fingerprint.cancelAuthentication();
        });
        fingerprint.start();
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
        password.start();
        QTRY_COMPARE_WITH_TIMEOUT(contents(dir.filePath(QStringLiteral("password"))), QByteArray("approved"), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!fingerprint.isActive(), 2500);
        QCOMPARE(errors.size(), 0);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("fingerprint"))));
        password.stop();
    }
    void deniedApprovalIsDeliveredToHelper() {
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 80);
        connect(&a, &Auth::authentication, this, [&](const QString &, bool) { a.setSessionApproved(false); });
        a.start();
        QTRY_COMPARE(contents(dir.filePath(QStringLiteral("proof"))), QByteArray("rejected"));
        QTRY_VERIFY(!a.isActive());
    }
    void oldUserSuccessIsRejected() {
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 100);
        AuthenticationSelection selection;
        selection.selectUser(QStringLiteral("other-user"));
        connect(&a, &Auth::authentication, this,
                [&](const QString &user, bool ok) { a.setSessionApproved(ok && selection.accept(&a, user)); });
        a.start();
        QTRY_COMPARE(contents(dir.filePath(QStringLiteral("proof"))), QByteArray("rejected"));
        QVERIFY(!selection.winner());
        a.stop();
    }
    void cancellationIsNonblockingAndBounded() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 4000, QStringLiteral("ignore-term"));
        QSignalSpy auth(&a, &Auth::authentication);
        a.start();
        QTest::qWait(120);
        QElapsedTimer elapsed;
        elapsed.start();
        a.cancelAuthentication();
        QVERIFY(elapsed.elapsed() < 100);
        QTRY_VERIFY_WITH_TIMEOUT(!a.isActive(), 2500);
        QCOMPARE(auth.size(), 0);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void cancellationDoesNotKillApprovedSession() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 80);
        a.start();
        QTRY_COMPARE(contents(dir.filePath(QStringLiteral("proof"))), QByteArray("approved"));
        a.cancelAuthentication();
        QTest::qWait(200);
        QVERIFY(a.isActive());
        a.stop();
    }
    void legacyFramingWorksWhenParallelDisabled() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 10, QStringLiteral("legacy"));
        a.start();
        QTRY_COMPARE(contents(dir.filePath(QStringLiteral("proof"))), QByteArray("approved"));
        a.stop();
    }
    void oldHelperCannotBypassParallelArbitration() {
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 10, QStringLiteral("legacy"));
        QSignalSpy authentication(&a, &Auth::authentication);
        a.start();
        QTRY_VERIFY_WITH_TIMEOUT(!a.isActive(), 3000);
        QCOMPARE(authentication.size(), 0);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void absentHandshakeIsBounded() {
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 10, QStringLiteral("no-hello"));
        a.start();
        QTRY_VERIFY_WITH_TIMEOUT(!a.isActive(), 6500);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void malformedHandshakeDoesNotBlockEventLoop_data() {
        QTest::addColumn<QString>("mode");
        for (const char *mode : {"partial-length", "partial-payload", "oversized", "disconnect", "post-partial-length", "post-partial-payload", "post-oversized", "post-disconnect"})
            QTest::newRow(mode) << QString::fromLatin1(mode);
    }
    void malformedHandshakeDoesNotBlockEventLoop() {
        QFETCH(QString, mode);
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 10, mode);
        bool fired = false;
        a.start();
        QTimer::singleShot(200, this, [&] { fired = true; });
        QTRY_VERIFY_WITH_TIMEOUT(fired, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!a.isActive(), 6500);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void unavailablePeerCredentialsDisableParallel() {
#if !SDDM_TRUSTED_HELPER_PID
        mainConfig.ParallelFingerprint.set(true);
        mainConfig.Users.ReuseSession.set(false);
        QVERIFY(!parallelFingerprintEnabled());
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 10);
        QSignalSpy finished(&a, &Auth::finished);
        a.start();
        QVERIFY(!a.isActive());
        QCOMPARE(finished.size(), 1);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
#endif
    }
    void runtimeListenerReentrancyIsSafe() {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        QObject first;
        auto *second = new QObject;
        int calls = 0, removedCalls = 0, addedCalls = 0;
        registerParallelRuntimeListener(&first, [&] {
            ++calls;
            delete second; second = nullptr;
            registerParallelRuntimeListener(&first, [&] { ++addedCalls; });
        });
        registerParallelRuntimeListener(second, [&] { ++removedCalls; });
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        QCOMPARE(calls, 1);
        QCOMPARE(removedCalls, 0);
        QCOMPARE(addedCalls, 0);
#endif
    }
    void cancelledPromptCanCompleteWithoutSocket() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 10, QStringLiteral("prompt"));
        QSignalSpy finished(&a, &Auth::finished);
        int prompts = 0;
        connect(&a, &Auth::requestChanged, &a, [&] {
            if (a.request()->prompts().isEmpty()) return;
            ++prompts;
            a.cancelAuthentication();
            a.request()->done();
        });
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(prompts, 1);
        QVERIFY(!a.isActive());
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("proof"))));
    }
    void legacySuccessCallbackMayCancelSafely() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 20);
        QSignalSpy finished(&a, &Auth::finished);
        connect(&a, &Auth::authentication, &a, [&a](const QString &, bool success) {
            if (success) a.cancelAuthentication();
        });
        a.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!a.isActive());
        QVERIFY(contents(dir.filePath(QStringLiteral("proof"))) != "approved");
    }
    void embeddedImageCannotBeMutated() {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        testParallelFailure(0);
        QVERIFY(testEmbeddedImageSealed());
#endif
    }
    void fastHelperHelloSurvivesStartedOrdering() {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        testParallelFailure(0);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        for (int i = 0; i < 30; ++i) {
            QTemporaryDir dir;
            Auth a;
            a.setPasswordOnly(true);
            setup(a, dir.filePath(QStringLiteral("proof")), 0);
            a.start();
            QTRY_COMPARE_WITH_TIMEOUT(contents(dir.filePath(QStringLiteral("proof"))), QByteArray("approved"), 3000);
            a.stop();
        }
#endif
    }
    void cancelledFailedStartClosesDescriptorsAndFinishesOnce() {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        testParallelFailure(10);
        QTRY_VERIFY_WITH_TIMEOUT(parallelRuntimeAvailable(), 4000);
        QTemporaryDir dir;
        Auth a;
        a.setPasswordOnly(true);
        setup(a, dir.filePath(QStringLiteral("proof")), 10);
        const int before = QDir(QStringLiteral("/proc/self/fd")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size();
        QSignalSpy finished(&a, &Auth::finished);
        a.start();
        a.cancelAuthentication();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QTest::qWait(100);
        QCOMPARE(finished.size(), 1);
        const int after = QDir(QStringLiteral("/proc/self/fd")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size();
        QCOMPARE(after, before);
        testParallelFailure(0);
#endif
    }
    void stoppedSelectionCannotAuthenticate() {
        AuthenticationSelection s;
        Auth a;
        s.selectUser(QStringLiteral("fixture"));
        s.close();
        QVERIFY(!s.accept(&a, QStringLiteral("fixture")));
        QVERIFY(!s.winner());
    }
    void helperCrashIsNotAuthenticationSuccess() {
        QTemporaryDir dir;
        Auth a;
        setup(a, dir.filePath(QStringLiteral("proof")), 80, QStringLiteral("crash"));
        QSignalSpy finished(&a, &Auth::finished), auth(&a, &Auth::authentication);
        a.start();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().value<Auth::HelperExitStatus>(), Auth::HELPER_OTHER_ERROR);
        QCOMPARE(auth.size(), 0);
    }
};
QTEST_GUILESS_MAIN(ParallelAuthTest)
#include "ParallelAuthTest.moc"
