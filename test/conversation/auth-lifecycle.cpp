#include <QtTest>
#include <QTimer>
#include <QProcess>
#include "Auth.h"
using namespace SDDM;

class AuthLifecycleProofs : public QObject {
    Q_OBJECT
private slots:
    void deferredDeletionDoesNotNotifyClearedOwner() {
        QObject owner;
        Auth *current = new Auth(&owner);
        int clearedOwnerNotifications = 0;
        connect(current, &Auth::requestChanged, &owner, [&] {
            // Greeter::onHelperFinished clears its Auth pointer before deferred deletion.
            // Its requestChanged slot must never be invoked after that lifetime cutoff.
            if (!current) ++clearedOwnerNotifications;
        });
        QPointer<Auth> pending = current;
        current->deleteLater();
        current = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(pending.isNull());
        QCOMPARE(clearedOwnerNotifications, 0);
    }
    void cancellationReturnsImmediatelyAndReapsAsynchronously() {
        Auth auth;
        auto *process = auth.findChild<QProcess *>();
        auto *killTimer = auth.findChild<QTimer *>();
        QVERIFY(process); QVERIFY(killTimer);
        process->start(QStringLiteral(CONVERSATION_PYTHON), {QStringLiteral("-c"),
            QStringLiteral("import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); print('ready',flush=True); time.sleep(30)")});
        QVERIFY(process->waitForReadyRead(3000));
        QCOMPARE(process->readAllStandardOutput(), QByteArray("ready\n"));
        QElapsedTimer elapsed; elapsed.start(); auth.stop();
        QVERIFY(elapsed.elapsed() < 250);
        QVERIFY(auth.isCancelled()); QVERIFY(killTimer->isActive()); QCOMPARE(killTimer->interval(), 5000);
        // Shorten only the already-armed test timer; production keeps its five-second bound.
        killTimer->setInterval(30);
        QTRY_COMPARE(process->state(), QProcess::NotRunning);
    }
    void destroyingActiveAuthLeavesOnlyBoundedReaper() {
        auto *auth = new Auth;
        auto *process = auth->findChild<QProcess *>();
        auto *killTimer = auth->findChild<QTimer *>();
        QVERIFY(process); QVERIFY(killTimer);
        process->start(QStringLiteral(CONVERSATION_PYTHON), {QStringLiteral("-c"),
            QStringLiteral("import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); print('ready',flush=True); time.sleep(30)")});
        QVERIFY(process->waitForReadyRead(3000));
        process->readAllStandardOutput(); killTimer->setInterval(30);
        QPointer<QProcess> pending = process;
        QElapsedTimer elapsed; elapsed.start(); delete auth;
        QVERIFY(elapsed.elapsed() < 250);
        QTRY_VERIFY(pending.isNull());
    }
    void explicitStopStillNotifiesAndInvalidates() {
        Auth auth;
        QSignalSpy requests(&auth, &Auth::requestChanged);
        auth.stop();
        QVERIFY(auth.isCancelled());
        QCOMPARE(requests.count(), 1);
        QVERIFY(auth.request()->prompts().isEmpty());
    }
};
QTEST_GUILESS_MAIN(AuthLifecycleProofs)
#include "auth-lifecycle.moc"
