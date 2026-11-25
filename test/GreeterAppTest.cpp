/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Configuration.h"
#include "GreeterApp.h"
#include "GreeterProxy.h"
#include "Messages.h"
#include "Session.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlContext>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

namespace SDDM {
namespace {
    const QString requestId = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    template<typename Writer> QByteArray packet(Writer writer) {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        writer(stream);
        return bytes;
    }
    struct Request {
        quint32 message { 0 }, hasContext { 0 }, method { 0 }, type { 0 };
        quint64 serial { 0 };
        QString user, file, id, password;
    };
}

class GreeterAppTest : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<GreeterApp> m_app;
    QLocalServer m_server;
    QLocalSocket *m_peer { nullptr };
    QVector<Request> m_requests;
    Request m_binding;
    bool m_decodeError { false };
    int m_startCount { 0 };

    bool writeFile(const QString &name, const QByteArray &contents) {
        QFile file(m_dir->filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }
    void send(const QByteArray &bytes) { m_peer->write(bytes); m_peer->flush(); }
    QByteArray ack(quint64 serial, bool clear, bool committed = false) {
        return packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationContext) << serial << quint32(!committed)
              << (clear && !committed ? QString() : requestId)
              << (clear && !committed ? QString() : m_binding.user)
              << quint32(clear && !committed ? quint32(Session::UnknownSession) : m_binding.type)
              << (clear && !committed ? QString() : m_binding.file)
              << quint32(clear && !committed ? AuthenticationAvailability::Unavailable : AuthenticationAvailability::Available)
              << quint32(committed ? AuthenticationPhase::SessionAuthorized : AuthenticationPhase::Idle)
              << (committed ? QStringLiteral("session_committed") : (clear ? QStringLiteral("cleared") : QStringLiteral("available")));
        });
    }
    void matched() {
        send(packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationMethodState) << m_binding.serial << requestId
              << quint32(AuthenticationMethod::Fingerprint) << m_binding.user << m_binding.type << m_binding.file
              << quint32(FingerprintState::Matched) << quint32(AuthenticationPhase::SessionAuthorized) << QStringLiteral("matched");
        }));
    }
    void complete(AuthenticationResult result) {
        send(packet([&](QDataStream &s) {
            s << quint32(DaemonMessages::AuthenticationAttemptFinished) << m_binding.serial << requestId
              << m_binding.user << m_binding.type << m_binding.file << quint32(result) << quint32(AuthenticationMethod::Fingerprint);
        }));
    }
    void readRequests() {
        QDataStream stream(m_peer);
        while (m_peer->bytesAvailable() >= qint64(sizeof(quint32))) {
            Request request;
            stream.startTransaction();
            stream >> request.message;
            switch (GreeterMessages(request.message)) {
                case GreeterMessages::Connect: break;
                case GreeterMessages::QueryAuthenticationCapabilities: break;
                case GreeterMessages::SetAuthenticationContext:
                    stream >> request.serial >> request.hasContext;
                    if (request.hasContext) stream >> request.user >> request.type >> request.file;
                    break;
                case GreeterMessages::StartAuthenticationMethod: stream >> request.serial >> request.method; break;
                case GreeterMessages::CancelAuthenticationMethod: stream >> request.serial >> request.id >> request.method; break;
                case GreeterMessages::Login: stream >> request.user >> request.password >> request.type >> request.file; break;
                default: m_decodeError = true; stream.abortTransaction(); return;
            }
            if (!stream.commitTransaction()) return;
            m_requests.append(request);
            if (GreeterMessages(request.message) == GreeterMessages::QueryAuthenticationCapabilities)
                send(packet([](QDataStream &s) { s << quint32(DaemonMessages::AuthenticationCapabilities) << quint32(2); }));
            else if (GreeterMessages(request.message) == GreeterMessages::SetAuthenticationContext && request.hasContext) {
                m_binding = request;
                send(ack(request.serial, false));
            } else if (GreeterMessages(request.message) == GreeterMessages::StartAuthenticationMethod) {
                ++m_startCount;
            }
        }
    }
    int count(GreeterMessages message) const {
        int result = 0;
        for (const auto &request : m_requests) if (request.message == quint32(message)) ++result;
        return result;
    }
    quint64 latestClear() const {
        for (auto i = m_requests.crbegin(); i != m_requests.crend(); ++i)
            if (i->message == quint32(GreeterMessages::SetAuthenticationContext) && !i->hasContext) return i->serial;
        return 0;
    }
    void forceLoadError() {
        m_app->m_controllerView->setSource(QUrl::fromLocalFile(m_dir->filePath(QStringLiteral("missing.qml"))));
    }

private slots:
    void initTestCase() {
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            m_peer = m_server.nextPendingConnection();
            connect(m_peer, &QLocalSocket::readyRead, this, &GreeterAppTest::readRequests);
            readRequests();
        });
    }
    void init() {
        QVERIFY(QGuiApplication::primaryScreen());
        m_dir = std::make_unique<QTemporaryDir>(QDir::currentPath() + QStringLiteral("/greeter-lifecycle-XXXXXX"));
        QVERIFY(m_dir->isValid());
        QVERIFY(writeFile(QStringLiteral("session.desktop"), "[Desktop Entry]\nName=Private session\nExec=/bin/true\nTryExec=/bin/true\nType=Application\n"));
        QVERIFY(writeFile(QStringLiteral("metadata.desktop"), "[SddmGreeterTheme]\nName=Lifecycle regression\nMainScript=Main.qml\nConfigFile=theme.conf\n"));
        QVERIFY(writeFile(QStringLiteral("theme.conf"), "[General]\nneedsFullUserModel=false\n"));
        QVERIFY(writeFile(QStringLiteral("Main.qml"), R"QML(
import QtQuick 2.15
Rectangle {
    id: root
    width: 640; height: 480
    property bool owner: authenticationController
    property bool started: false
    readonly property bool controlsAllowed: owner && !sddm.authenticationBusy && sddm.authenticationSubmissionAllowed
    function select() {
        if (owner && sddm.authenticationMethodsAvailable && !sddm.authenticationBusy)
            sddm.setAuthenticationContext("alice", 0)
    }
    function start() {
        if (owner && !started && sddm.authenticationUser === "alice"
                && sddm.fingerprintRequestId.length > 0 && sddm.fingerprintAvailable
                && !sddm.authenticationBusy) {
            started = true
            sddm.startFingerprint("alice", 0)
        }
    }
    onOwnerChanged: if (owner) { started = false; select(); start() }
    Component.onCompleted: { select(); start() }
    Connections {
        target: sddm
        function onAuthenticationMethodsAvailableChanged() { root.select() }
        function onFingerprintRequestIdChanged() { root.start() }
        function onFingerprintAvailableChanged() { root.start() }
    }
}
)QML"));
        mainConfig.X11.SessionDir.set(QStringList{m_dir->path()});
        mainConfig.Wayland.SessionDir.set(QStringList{});
        m_startCount = 0;
        m_requests.clear();
        m_decodeError = false;
        QVERIFY(m_server.listen(m_dir->filePath(QStringLiteral("peer"))));
        m_app = std::make_unique<GreeterApp>();
        m_app->setTestModeEnabled(true);
        m_app->setSocketName(m_server.fullServerName());
        m_app->setThemePath(m_dir->path());
        m_app->startup();
        QTRY_COMPARE(m_startCount, 1);
        QVERIFY(m_app->m_controllerView);
        QCOMPARE(m_app->m_controllerView->status(), QQuickView::Ready);
    }
    void cleanup() {
        QVERIFY(!m_decodeError);
        if (m_app && m_app->m_proxy) disconnect(m_app->m_proxy, nullptr, qGuiApp, nullptr);
        m_app.reset();
        delete m_peer;
        m_peer = nullptr;
        m_server.close();
        m_dir.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void partialActivationThenWholeGreeterFallback() {
        m_app->addViewForScreen(QGuiApplication::primaryScreen());
        QCOMPARE(m_app->m_views.size(), 2);
        QPointer<QQuickView> first = m_app->m_views.first();
        QPointer<QQuickView> second = m_app->m_views.last();
        forceLoadError();
        QTRY_VERIFY(latestClear() != 0);
        const auto clear = latestClear();
        QTRY_VERIFY(m_app->m_embeddedFallback);
        QTRY_VERIFY(first.isNull());
        QTRY_VERIFY(second.isNull());
        QCOMPARE(m_app->themePath(), QStringLiteral("qrc:/theme"));
        QCOMPARE(m_app->m_views.size(), QGuiApplication::screens().size());
        QQuickView *fallback = m_app->m_views.first();
        QTRY_COMPARE(fallback->status(), QQuickView::Ready);
        QVERIFY(fallback->rootObject());
        QVERIFY(!m_app->m_proxy->authenticationSubmissionAllowed());
        QVERIFY(!fallback->rootObject()->property("canSubmit").toBool());
        QVERIFY(QMetaObject::invokeMethod(fallback->rootObject(), "submit"));
        QTest::keyClick(fallback, Qt::Key_Return);
        QTest::qWait(10);
        QCOMPARE(count(GreeterMessages::Login), 0);
        QCOMPARE(m_startCount, 1);
        send(ack(clear, true));
        QTRY_VERIFY(m_app->m_proxy->authenticationSubmissionAllowed());
        QTRY_VERIFY(fallback->rootContext()->contextProperty(QStringLiteral("authenticationController")).toBool());
        fallback->rootObject()->setProperty("selectedUser", QStringLiteral("alice"));
        QVERIFY(QMetaObject::invokeMethod(fallback->rootObject(), "submit"));
        QTRY_COMPARE(count(GreeterMessages::Login), 1);
        QCOMPARE(m_requests.last().message, quint32(GreeterMessages::Login));
        QCOMPARE(m_startCount, 1);
    }

    void committedFallbackKeepsIdentityUntilFailureAndFreshClear() {
        matched();
        QTRY_VERIFY(m_app->m_proxy->authenticationBusy());
        forceLoadError();
        QTRY_VERIFY(latestClear() != 0);
        const auto clear = latestClear();
        QTRY_VERIFY(m_app->m_embeddedFallback);
        send(ack(clear, true, true));
        QTRY_COMPARE(m_app->m_proxy->authenticationUser(), QStringLiteral("alice"));
        QCOMPARE(m_app->m_proxy->fingerprintRequestId(), requestId);
        QVERIFY(m_app->m_proxy->authenticationBusy());
        QVERIFY(!m_app->m_proxy->authenticationSubmissionAllowed());
        QVERIFY(m_app->m_controllerView);
        QVERIFY(m_app->m_controllerView->rootObject());
        QVERIFY(!m_app->m_controllerView->rootObject()->property("canSubmit").toBool());
        complete(AuthenticationResult::Failed);
        QTRY_VERIFY(latestClear() > clear);
        const auto fresh = latestClear();
        QVERIFY(!m_app->m_proxy->authenticationSubmissionAllowed());
        send(ack(clear, true));
        QTest::qWait(10);
        QVERIFY(!m_app->m_proxy->authenticationSubmissionAllowed());
        send(ack(fresh, true));
        QTRY_VERIFY(m_app->m_proxy->authenticationSubmissionAllowed());
        QCOMPARE(count(GreeterMessages::Login), 0);
        QCOMPARE(m_startCount, 1);
    }

    void mirrorDestructionHarmlessAndPrecommitPromotionWaits() {
        m_app->addViewForScreen(QGuiApplication::primaryScreen());
        QQuickView *mirror = m_app->m_views.last();
        QQuickView *controller = m_app->m_controllerView;
        const int contexts = count(GreeterMessages::SetAuthenticationContext);
        m_app->removeViewForScreen(mirror);
        QTest::qWait(10);
        QCOMPARE(count(GreeterMessages::SetAuthenticationContext), contexts);
        QCOMPARE(m_app->m_controllerView.data(), controller);
        QCOMPARE(m_startCount, 1);
        m_app->addViewForScreen(QGuiApplication::primaryScreen());
        mirror = m_app->m_views.last();
        m_app->removeViewForScreen(controller);
        QTRY_VERIFY(latestClear() != 0);
        QVERIFY(!m_app->m_controllerView);
        QVERIFY(!mirror->rootContext()->contextProperty(QStringLiteral("authenticationController")).toBool());
        QCOMPARE(m_startCount, 1);
        send(ack(latestClear(), true));
        QTRY_COMPARE(m_app->m_controllerView.data(), mirror);
        QTRY_COMPARE(m_startCount, 2);
    }

    void postcommitPromotionRetainsIdentityAndNeverRestarts() {
        m_app->addViewForScreen(QGuiApplication::primaryScreen());
        QQuickView *mirror = m_app->m_views.last();
        matched();
        QTRY_VERIFY(m_app->m_proxy->authenticationBusy());
        m_app->removeViewForScreen(m_app->m_controllerView);
        QTRY_VERIFY(latestClear() != 0);
        send(ack(latestClear(), true, true));
        QTRY_COMPARE(m_app->m_controllerView.data(), mirror);
        QCOMPARE(m_app->m_proxy->authenticationUser(), QStringLiteral("alice"));
        QCOMPARE(m_app->m_proxy->fingerprintRequestId(), requestId);
        QVERIFY(!mirror->rootObject()->property("controlsAllowed").toBool());
        QTest::qWait(10);
        QCOMPARE(m_startCount, 1);
        QSignalSpy success(m_app->m_proxy, &GreeterProxy::loginSucceeded);
        complete(AuthenticationResult::Succeeded);
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(m_app->m_proxy->authenticationBusy());
        QCOMPARE(m_startCount, 1);
    }
};
}

QTEST_MAIN(SDDM::GreeterAppTest)
#include "GreeterAppTest.moc"
