// Exercises production Display/SocketServer/Auth with private unprivileged peers.
#include "Display.h"
#include "SocketServer.h"
#include "Configuration.h"
#include "GreeterProxy.h"
#include "SessionModel.h"

#include <QDataStream>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusPendingReply>
#include <QProcess>
#include <memory>
#include <functional>

namespace SDDM {
class DaemonContextTest : public QObject {
    Q_OBJECT
    struct Event {
        DaemonMessages message;
        quint64 serial = 0;
        quint32 accepted = 0, state = 0, phase = 0, availability = 0, result = 0, method = 0;
        QString request, user, file, detail;
    };
    struct Client {
        QLocalSocket socket;
        QByteArray pending;
        QList<Event> events;
        bool malformed = false;
        std::function<void(const Event &)> consume;
        Client() {
            QObject::connect(&socket, &QLocalSocket::readyRead, &socket, [this] {
                pending.append(socket.readAll());
                while (!pending.isEmpty()) {
                    QDataStream input(pending);
                    quint32 message = 0, type = 0;
                    Event event;
                    input >> message;
                    if (input.status() == QDataStream::ReadPastEnd)
                        return;
                    event.message = DaemonMessages(message);
                    switch (event.message) {
                    case DaemonMessages::AuthenticationCapabilities:
                        input >> event.availability;
                        break;
                    case DaemonMessages::AuthenticationContext:
                        input >> event.serial >> event.accepted >> event.request >> event.user >> type >> event.file
                              >> event.availability >> event.phase >> event.detail;
                        break;
                    case DaemonMessages::AuthenticationMethodState:
                        input >> event.serial >> event.request >> event.method >> event.user >> type >> event.file
                              >> event.state >> event.phase >> event.detail;
                        break;
                    case DaemonMessages::AuthenticationAttemptFinished:
                        input >> event.serial >> event.request >> event.user >> type >> event.file >> event.result >> event.method;
                        break;
                    case DaemonMessages::LoginFailed:
                    case DaemonMessages::LoginSucceeded:
                        break;
                    case DaemonMessages::InformationMessage:
                        input >> event.detail;
                        break;
                    default:
                        malformed = true;
                        return;
                    }
                    if (input.status() == QDataStream::ReadPastEnd)
                        return;
                    if (input.status() != QDataStream::Ok) { malformed = true; return; }
                    pending.remove(0, input.device()->pos());
                    events.append(event);
                    if (consume)
                        consume(event);
                }
            });
        }
        int count(DaemonMessages message, const QString &detail = QString()) const {
            int total = 0;
            for (const auto &event : events)
                total += event.message == message && (detail.isEmpty() || event.detail == detail);
            return total;
        }
        bool state(FingerprintState state, AuthenticationMethod method = AuthenticationMethod::Fingerprint) const {
            for (const auto &event : events)
                if (event.message == DaemonMessages::AuthenticationMethodState && event.state == quint32(state)
                    && event.method == quint32(method))
                    return true;
            return false;
        }
    };
    struct Control {
        QPointer<QLocalSocket> socket;
        QString request;
        bool authorized = false;
    };
    class Harness final : public Display {
    public:
        QLocalServer controls;
        QHash<int, Control> peers;
        QString fingerprintMode = QStringLiteral("ignore-term");
        QString passwordMode = QStringLiteral("ignore-term");
        int probes = 0;
        bool realAvailability = false;
        explicit Harness(const QString &controlPath) : Display(nullptr, WaylandDisplayServerType, -1) {
            controls.listen(controlPath);
            QObject::connect(&controls, &QLocalServer::newConnection, this, [this] {
                auto *socket = controls.nextPendingConnection();
                QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                    while (socket->canReadLine()) {
                        const auto line = socket->readLine().trimmed();
                        if (line.startsWith("READY ")) {
                            const auto fields = line.split(' ');
                            if (fields.size() == 3)
                                peers.insert(fields.at(1).toInt(), {socket, QString::fromUtf8(fields.at(2)), false});
                        } else if (line == "AUTHORIZED") {
                            for (auto it = peers.begin(); it != peers.end(); ++it)
                                if (it->socket == socket)
                                    it->authorized = true;
                        }
                    }
                });
                QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            });
        }
        void command(AuthMethod method, const QByteArray &bytes) {
            const auto peer = peers.value(int(method)).socket;
            if (peer) { peer->write(bytes); peer->flush(); }
        }
    private:
        void probeFingerprintAvailability() override {
            if (realAvailability) {
                Display::probeFingerprintAvailability();
                return;
            }
            // Delivery is controlled here; no system-bus service or reader is
            // a prerequisite for the production lifecycle cases.
            invalidateProbe();
            ++probes;
            m_fingerprintAvailability = AuthenticationAvailability::Unknown;
        }
        bool startAuth(const QString &user, const QString &password, const Session &, AuthMethod method) override {
            Auth *auth = method == AuthMethod::Fingerprint ? m_fingerprintAuth : m_auth;
            if (auth->isActive())
                return false;
            auth->setMethod(method);
            auth->setRequestId(m_requestId);
            auth->setDeferredAuthorization(true);
            auth->setUser(user);
            auth->setSession(method == AuthMethod::Fingerprint ? fingerprintMode : passwordMode);
            auth->setDisplayServerCommand(controls.fullServerName());
            if (method == AuthMethod::Password)
                m_passPhrase = password;
            m_attemptGate.resume(method);
            if (method == AuthMethod::Fingerprint)
                m_fingerprintGeneration = auth->launchGeneration() + 1;
            else
                m_passwordGeneration = auth->launchGeneration() + 1;
            peers.remove(int(method));
            auth->start();
            if (method == AuthMethod::Fingerprint && auth->isActive() && !m_fingerprintTerminal)
                armFingerprintDeadline(auth);
            return true;
        }
    };
    std::unique_ptr<QTemporaryDir> directory;
    Session session;
    std::unique_ptr<QTemporaryDir> busDirectory;
    QProcess privateBus;
    QStringList fixtureOwners;
    QString startupGate;
    QString serviceFile() const {
        return busDirectory->filePath(QStringLiteral("services/net.reactivated.Fprint.service"));
    }
    void enableService(const QStringList &args = {}) {
        const int gateIndex = args.indexOf(QStringLiteral("--startup-gate"));
        if (gateIndex >= 0 && gateIndex + 1 < args.size())
            startupGate = args.at(gateIndex + 1);
        QFile service(serviceFile());
        QVERIFY(service.open(QIODevice::WriteOnly));
        const auto executable = QCoreApplication::applicationDirPath() + QStringLiteral("/FingerprintAvailabilityService");
        service.write(QStringLiteral("[D-BUS Service]\nName=net.reactivated.Fprint\nExec=%1 %2\n")
                      .arg(executable, args.join(QLatin1Char(' '))).toUtf8());
        service.close();
        const auto reply = QDBusConnection::systemBus().interface()->call(QStringLiteral("ReloadConfig"));
        QVERIFY(reply.type() != QDBusMessage::ErrorMessage);
    }
    QString fixtureOwner() {
        const QDBusReply<QString> owner = QDBusConnection::systemBus().interface()->serviceOwner(
                QStringLiteral("net.reactivated.Fprint"));
        if (owner.isValid() && !fixtureOwners.contains(owner.value()))
            fixtureOwners.append(owner.value());
        return owner.isValid() ? owner.value() : QString();
    }
    QDBusMessage fixtureCall(const QString &owner, const QString &method, bool device = false) {
        return QDBusConnection::systemBus().call(QDBusMessage::createMethodCall(owner,
                device ? QStringLiteral("/net/reactivated/Fprint/Device/0")
                       : QStringLiteral("/net/reactivated/Fprint/Manager"),
                device ? QStringLiteral("net.reactivated.Fprint.Device")
                       : QStringLiteral("net.reactivated.Fprint.Manager"), method));
    }
    int pendingEnrollment(const QString &owner) {
        const auto reply = fixtureCall(owner, QStringLiteral("PendingReplies"), true);
        return reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                ? reply.arguments().constFirst().toInt() : -1;
    }
    template<typename Writer> static QByteArray packet(Writer write) {
        QByteArray bytes;
        QDataStream output(&bytes, QIODevice::WriteOnly);
        write(output);
        return bytes;
    }
    template<typename Writer> static void send(Client &client, Writer write) {
        client.socket.write(packet(write));
        client.socket.flush();
    }
    void connectClient(Harness &display, Client &client) {
        if (display.m_socketServer->socketAddress().isEmpty())
            QVERIFY(display.m_socketServer->start(QStringLiteral("daemon-context-test")));
        client.socket.connectToServer(display.m_socketServer->socketAddress());
        QTRY_COMPARE(client.socket.state(), QLocalSocket::ConnectedState);
        send(client, [](QDataStream &output) { output << quint32(GreeterMessages::QueryAuthenticationCapabilities); });
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationCapabilities), 1);
    }
    void select(Client &client, quint64 serial, const QString &user) {
        send(client, [&](QDataStream &output) {
            output << quint32(GreeterMessages::SetAuthenticationContext) << serial << quint32(1) << user << session;
        });
    }
    static void clear(Client &client, quint64 serial) {
        send(client, [&](QDataStream &output) {
            output << quint32(GreeterMessages::SetAuthenticationContext) << serial << quint32(0);
        });
    }
    static void start(Client &client, quint64 serial) {
        send(client, [&](QDataStream &output) {
            output << quint32(GreeterMessages::StartAuthenticationMethod) << serial << quint32(AuthenticationMethod::Fingerprint);
        });
    }
    static void cancel(Client &client, quint64 serial, const QString &request) {
        send(client, [&](QDataStream &output) {
            output << quint32(GreeterMessages::CancelAuthenticationMethod) << serial << request
                   << quint32(AuthenticationMethod::Fingerprint);
        });
    }
    void password(Client &client, const QString &user, const QString &value = QStringLiteral("private-test-response")) {
        send(client, [&](QDataStream &output) { output << quint32(GreeterMessages::Login) << user << value << session; });
    }
    void activate(Harness &display, Client &client, const QString &user) {
        connectClient(display, client);
        select(client, 1, user);
        QTRY_COMPARE(display.m_contextUser, user);
        display.setFingerprintAvailability(1, display.m_requestId, true, QStringLiteral("available"));
        start(client, 1);
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Fingerprint)).request, display.m_requestId);
        QTRY_VERIFY(client.state(FingerprintState::Scanning));
    }
private slots:
    void initTestCase() {
        busDirectory = std::make_unique<QTemporaryDir>(QDir::currentPath() + QStringLiteral("/availability-bus-XXXXXX"));
        QVERIFY(busDirectory->isValid());
        QVERIFY(QDir().mkdir(busDirectory->filePath(QStringLiteral("services"))));
        const auto address = QStringLiteral("unix:path=") + busDirectory->filePath(QStringLiteral("bus.socket"));
        QFile config(busDirectory->filePath(QStringLiteral("bus.conf")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(QStringLiteral("<busconfig><type>session</type><listen>%1</listen><servicedir>%2</servicedir>"
                "<policy context=\"default\"><allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
                "<allow own=\"*\"/><allow user=\"*\"/></policy></busconfig>")
                .arg(address, busDirectory->filePath(QStringLiteral("services"))).toUtf8());
        config.close();
        qputenv("DBUS_SYSTEM_BUS_ADDRESS", address.toUtf8());
        privateBus.start(QStringLiteral("dbus-daemon"),
                {QStringLiteral("--nofork"), QStringLiteral("--print-address=1"),
                 QStringLiteral("--config-file=") + config.fileName()});
        QVERIFY(privateBus.waitForStarted());
        QVERIFY(privateBus.waitForReadyRead());
        QVERIFY(privateBus.readLine().startsWith("unix:"));
        QVERIFY(QDBusConnection::systemBus().isConnected());
    }
    void cleanup() {
        // Cleanup may start an unused fixture to drain activation before reusing the private bus.
        if (!startupGate.isEmpty()) {
            QFile release(startupGate);
            if (!release.open(QIODevice::WriteOnly))
                qFatal("Cannot release private availability fixture");
            release.close();
            auto activate = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                    QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
                    QStringLiteral("StartServiceByName"));
            activate << QStringLiteral("net.reactivated.Fprint") << quint32(0);
            QDBusPendingReply<quint32> pending = QDBusConnection::systemBus().asyncCall(activate, 6000);
            if (!QTest::qWaitFor([&] { return pending.isFinished(); }, 6500)
                || pending.isError() || (pending.value() != 1 && pending.value() != 2)
                || fixtureOwner().isEmpty())
                qFatal("Private gated activation did not drain; refusing to reuse the bus");
            startupGate.clear();
        }
        const auto current = fixtureOwner();
        Q_UNUSED(current);
        if (QFile::exists(serviceFile()) && !QFile::remove(serviceFile()))
            qFatal("Cannot remove private availability fixture service");
        const auto reload = QDBusConnection::systemBus().interface()->call(QStringLiteral("ReloadConfig"));
        if (reload.type() == QDBusMessage::ErrorMessage)
            qFatal("Cannot disable private availability fixture activation");
        for (const auto &owner : qAsConst(fixtureOwners)) {
            fixtureCall(owner, QStringLiteral("Quit"));
            if (!QTest::qWaitFor([&] {
                    const auto registered = QDBusConnection::systemBus().interface()->isServiceRegistered(owner);
                    return registered.isValid() && !registered.value();
                }, 6500))
                qFatal("Private availability fixture connection survived cleanup");
        }
        fixtureOwners.clear();
        const auto registered = QDBusConnection::systemBus().interface()->isServiceRegistered(
                QStringLiteral("net.reactivated.Fprint"));
        if (!registered.isValid() || registered.value())
            qFatal("Private availability fixture still owns its service after cleanup");
    }
    void cleanupTestCase() {
        privateBus.terminate();
        QVERIFY(privateBus.waitForFinished());
    }
    void init() {
        directory = std::make_unique<QTemporaryDir>(QDir::currentPath() + QStringLiteral("/daemon-context-XXXXXX"));
        QVERIFY(directory->isValid());
        QFile desktop(directory->filePath(QStringLiteral("test.desktop")));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nName=Test\nExec=/usr/bin/true\nType=Application\n");
        desktop.close();
        mainConfig.X11.SessionDir.set({directory->path()});
        mainConfig.FingerprintAuthTimeout.set(1);
        session.setTo(Session::X11Session, QStringLiteral("test.desktop"));
        QVERIFY(session.isValid());
    }
    void activationFailureReportsUnavailable() {
        enableService({QStringLiteral("--fail-start")});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        start(client, 1);
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("service_unavailable")), 1);
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
    }
    void activationDeadlineRejectsLateOwnerAcquisition() {
        const auto gate = directory->filePath(QStringLiteral("late-start"));
        enableService({QStringLiteral("--startup-gate"), gate});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        start(client, 1);
        QTRY_VERIFY(QFile::exists(gate + QStringLiteral(".waiting")));
        QTRY_COMPARE_WITH_TIMEOUT(client.count(DaemonMessages::AuthenticationContext,
                QStringLiteral("availability_unavailable")), 1, 6500);
        QFile release(gate);
        QVERIFY(release.open(QIODevice::WriteOnly));
        release.close();
        QTRY_VERIFY(!fixtureOwner().isEmpty());
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
        QVERIFY(!client.state(FingerprintState::Ready));
    }
    void abandonedGatedActivationIsCleanedBeforeColdStart_data() {
        QTest::addColumn<bool>("waitForChild");
        QTest::newRow("child-waiting") << true;
        QTest::newRow("startup-pending") << false;
    }
    void abandonedGatedActivationIsCleanedBeforeColdStart() {
        QFETCH(bool, waitForChild);
        const auto gate = directory->filePath(QStringLiteral("abandoned-start"));
        enableService({QStringLiteral("--startup-gate"), gate});
        auto activate = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("StartServiceByName"));
        activate << QStringLiteral("net.reactivated.Fprint") << quint32(0);
        auto pending = QDBusConnection::systemBus().asyncCall(activate);
        if (waitForChild)
            QTRY_VERIFY(QFile::exists(gate + QStringLiteral(".waiting")));
        QVERIFY(!QFile::exists(gate));
        cleanup();
        QFile waiting(gate + QStringLiteral(".waiting"));
        QVERIFY(waiting.open(QIODevice::ReadOnly));
        const auto abandonedOwner = QString::fromUtf8(waiting.readAll());
        QVERIFY(abandonedOwner.startsWith(QLatin1Char(':')));
        QTRY_VERIFY(pending.isFinished());
        QVERIFY(!pending.isError());
        QVERIFY(!QDBusConnection::systemBus().interface()->isServiceRegistered(abandonedOwner).value());
        coldAvailabilityActivatesOnceAndStartsQueuedFingerprint();
        QVERIFY(!fixtureOwner().isEmpty());
        QVERIFY(fixtureOwner() != abandonedOwner);
    }
    void coldAvailabilityActivatesOnceAndStartsQueuedFingerprint() {
        enableService();
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        start(client, 1);
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Fingerprint)));
        QVERIFY(!fixtureOwner().isEmpty());
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Available);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(1));
        QTRY_VERIFY(client.state(FingerprintState::Scanning));
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
    }
    void missingServiceCanRecoverOnExplicitRetryAndAfterOwnerExit() {
        mainConfig.FingerprintAuthTimeout.set(30);
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("service_unavailable")), 1);
        const auto request = display.m_requestId;
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
        enableService();
        start(client, 1);
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Fingerprint)));
        QCOMPARE(display.m_requestId, request);
        const auto owner = fixtureOwner();
        QVERIFY(!owner.isEmpty());
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        fixtureCall(owner, QStringLiteral("Quit"));
        QTRY_VERIFY(fixtureOwner().isEmpty());
        QTRY_COMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        start(client, 1);
        QTRY_COMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Fingerprint)));
        QCOMPARE(display.m_requestId, request);
        QVERIFY(!fixtureOwner().isEmpty());
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
    }
    void unavailableActivationDoesNotBlockPassword() {
        enableService({QStringLiteral("--startup-gate"), directory->filePath(QStringLiteral("never-open"))});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        QTRY_VERIFY(QFile::exists(directory->filePath(QStringLiteral("never-open.waiting"))));
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unknown);
        QFile release(directory->filePath(QStringLiteral("never-open")));
        QVERIFY(release.open(QIODevice::WriteOnly));
        release.close();
        QTRY_COMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Available);
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        display.command(AuthMethod::Password, "E");
        QTRY_VERIFY(!display.m_auth->isActive());
    }
    void staleEnrollmentCannotRestoreReplacedContext() {
        enableService({QStringLiteral("--hold-enrollment")});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        QTRY_VERIFY(!fixtureOwner().isEmpty());
        const auto owner = fixtureOwner();
        QTRY_COMPARE(pendingEnrollment(owner), 1);
        start(client, 1);
        select(client, 2, QStringLiteral("b"));
        QTRY_COMPARE(pendingEnrollment(owner), 2);
        fixtureCall(owner, QStringLiteral("ReleaseReplies"), true);
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("not_enrolled")), 1);
        QCOMPARE(display.m_contextUser, QStringLiteral("b"));
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
        for (const auto &event : client.events)
            QVERIFY(event.availability != quint32(AuthenticationAvailability::Available));
    }
    void replacedOwnerCannotCompleteOldProbeOrAutomaticallyRestart() {
        mainConfig.FingerprintAuthTimeout.set(30);
        enableService({QStringLiteral("--hold-enrollment")});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        start(client, 1);
        QTRY_VERIFY(!fixtureOwner().isEmpty());
        const auto oldOwner = fixtureOwner();
        QTRY_COMPARE(pendingEnrollment(oldOwner), 1);
        QProcess replacement;
        replacement.start(QCoreApplication::applicationDirPath() + QStringLiteral("/FingerprintAvailabilityService"),
                          {QStringLiteral("--replace")});
        QVERIFY(replacement.waitForStarted());
        QTRY_VERIFY(!fixtureOwner().isEmpty() && fixtureOwner() != oldOwner);
        const auto newOwner = fixtureOwner();
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("service_unavailable")), 1);
        fixtureCall(oldOwner, QStringLiteral("ReleaseReplies"), true);
        QTest::qWait(50);
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
        start(client, 1);
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Fingerprint)));
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Available);
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        fixtureCall(newOwner, QStringLiteral("Quit"));
        QVERIFY(replacement.waitForFinished());
    }
    void availabilityDeadlineRejectsLateEnrollment() {
        enableService({QStringLiteral("--hold-enrollment")});
        Harness display(directory->filePath(QStringLiteral("control")));
        display.realAvailability = true;
        Client client;
        connectClient(display, client);
        select(client, 1, QStringLiteral("a"));
        start(client, 1);
        QTRY_VERIFY(!fixtureOwner().isEmpty());
        const auto owner = fixtureOwner();
        QTRY_COMPARE(pendingEnrollment(owner), 1);
        QTRY_COMPARE_WITH_TIMEOUT(client.count(DaemonMessages::AuthenticationContext,
                QStringLiteral("availability_unavailable")), 1, 6500);
        fixtureCall(owner, QStringLiteral("ReleaseReplies"), true);
        QTest::qWait(50);
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unavailable);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(0));
        QVERIFY(!client.state(FingerprintState::Ready));
    }
    void fragmentedContextAndSocketIsolation() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client owner, stranger;
        connectClient(display, owner);
        const auto bytes = packet([&](QDataStream &output) {
            output << quint32(GreeterMessages::SetAuthenticationContext) << quint64(1) << quint32(1)
                   << QStringLiteral("a") << session;
        });
        owner.socket.write(bytes.left(bytes.size() - 1));
        owner.socket.flush();
        QTest::qWait(20);
        QVERIFY(!display.m_contextSocket);
        QVERIFY(owner.count(DaemonMessages::AuthenticationContext) == 0);
        owner.socket.write(bytes.right(1));
        owner.socket.flush();
        QTRY_COMPARE(display.m_contextUser, QStringLiteral("a"));
        display.setFingerprintAvailability(1, display.m_requestId, true, QStringLiteral("available"));
        start(owner, 1);
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Fingerprint)));
        const auto request = display.m_requestId;
        const auto generation = display.m_fingerprintAuth->launchGeneration();
        connectClient(display, stranger);
        select(stranger, 1, QStringLiteral("b"));
        QTRY_COMPARE(stranger.count(DaemonMessages::AuthenticationContext, QStringLiteral("context_owned")), 1);
        cancel(stranger, 1, request);
        cancel(owner, 2, request);
        cancel(owner, 1, QStringLiteral("unknown-uuid"));
        start(owner, 1);
        QTest::qWait(20);
        QCOMPARE(display.m_requestId, request);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), generation);
        QVERIFY(!display.m_attemptGate.cancelled(AuthMethod::Fingerprint));
        QVERIFY(!stranger.state(FingerprintState::Scanning));
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        QVERIFY(!owner.malformed && !stranger.malformed);
    }
    void latestReplacementAndClearBarrier() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        const auto oldRequest = display.m_requestId;
        select(client, 2, QStringLiteral("b"));
        start(client, 2);
        QTRY_COMPARE(display.m_pendingContextSerial, quint64(2));
        select(client, 3, QStringLiteral("a"));
        start(client, 3);
        password(client, QStringLiteral("a"));
        cancel(client, 2, QString());
        QTRY_COMPARE(display.m_pendingContextSerial, quint64(3));
        QVERIFY(display.m_pendingFingerprintStart);
        QVERIFY(display.m_pendingLogin);
        QVERIFY(display.m_fingerprintAuth->isActive());
        for (const auto &event : client.events)
            QVERIFY(event.serial != 3);
        display.command(AuthMethod::Fingerprint, "ISE");
        QTRY_COMPARE(display.m_contextSerial, quint64(3));
        QTRY_VERIFY(display.m_auth->isActive());
        QVERIFY(display.m_requestId != oldRequest);
        display.setFingerprintAvailability(3, display.m_requestId, true, QStringLiteral("available"));
        QTRY_COMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Fingerprint)).request, display.m_requestId);
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Password)).request, display.m_requestId);
        QVERIFY(!display.m_contextCommitted);
        display.setFingerprintAvailability(1, oldRequest, true, QStringLiteral("available"));
        QCOMPARE(display.m_contextSerial, quint64(3));
        clear(client, 4);
        QTRY_VERIFY(display.m_pendingContext);
        QVERIFY(display.m_socketServer->isCorrelated(display.m_contextSocket));
        QCOMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 0);
        display.command(AuthMethod::Fingerprint, "IE");
        display.command(AuthMethod::Password, "IE");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
        QVERIFY(!display.m_auth->isActive() && !display.m_fingerprintAuth->isActive());
        const int eventCount = client.events.size();
        emit display.m_auth->info(QStringLiteral("obsolete private feedback"), Auth::INFO_UNKNOWN);
        emit display.m_auth->error(QStringLiteral("obsolete private error"), Auth::ERROR_AUTHENTICATION);
        QTest::qWait(20);
        QCOMPARE(client.events.size(), eventCount);
        const auto barrier = client.events.last();
        QCOMPARE(barrier.serial, quint64(4));
        QVERIFY(barrier.request.isEmpty() && barrier.user.isEmpty() && barrier.file.isEmpty());
        QCOMPARE(barrier.availability, quint32(AuthenticationAvailability::Unavailable));
        QVERIFY(!client.malformed);
    }
    void cancelBeforeAcknowledgement() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        select(client, 2, QStringLiteral("b"));
        start(client, 2);
        cancel(client, 2, QString());
        QTRY_VERIFY(display.m_pendingFingerprintCancelled);
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_COMPARE(display.m_contextSerial, quint64(2));
        QTRY_VERIFY(client.state(FingerprintState::Cancelled));
        display.setFingerprintAvailability(2, display.m_requestId, true, QStringLiteral("available"));
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(1));
        QVERIFY(!display.m_fingerprintAuth->isActive());
        start(client, 2);
        QTRY_VERIFY(display.m_fingerprintStartRequested);
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unknown);
        display.setFingerprintAvailability(2, display.m_requestId, true, QStringLiteral("available"));
        QTRY_COMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Fingerprint)).request, display.m_requestId);
        cancel(client, 1, QString());
        QTest::qWait(20);
        QVERIFY(!display.m_attemptGate.cancelled(AuthMethod::Fingerprint));
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
    }
    void unavailableSurvivesObservedExit() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        display.command(AuthMethod::Fingerprint, "UE");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationMethodState, QStringLiteral("authentication_unavailable")), 1);
        QCOMPARE(display.m_fingerprintTerminalState, FingerprintState::Unavailable);
        QVERIFY(!client.state(FingerprintState::Failed));
        QVERIFY(!display.m_fingerprintDeadline);
        QVERIFY(!display.m_contextCommitted);
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.m_auth->isActive());
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        display.command(AuthMethod::Password, "E");
        QTRY_VERIFY(!display.m_auth->isActive());
    }
    void timeoutFirstRejectsLateSuccess() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        QVERIFY(display.m_fingerprintDeadline);
        QVERIFY(QMetaObject::invokeMethod(display.m_fingerprintDeadline, "timeout", Qt::DirectConnection));
        QVERIFY(display.m_fingerprintAuth->isActive());
        QVERIFY(!client.state(FingerprintState::TimedOut));
        display.command(AuthMethod::Fingerprint, "S");
        QTest::qWait(20);
        QVERIFY(!display.m_methodWinnerChosen && !display.m_contextCommitted);
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        QTRY_VERIFY(client.state(FingerprintState::TimedOut));
        QVERIFY(!client.state(FingerprintState::Matched));
        start(client, 1);
        QTRY_COMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unknown);
        display.setFingerprintAvailability(1, display.m_requestId, true, QStringLiteral("available"));
        QTRY_COMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        QTRY_VERIFY(display.m_fingerprintAwaitingAuthentication);
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Fingerprint)).request, display.m_requestId);
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
    }
    void successFirstSurvivesLoserAndAccountWork() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        display.command(AuthMethod::Fingerprint, "S");
        QTRY_VERIFY(display.m_waitingForLoser);
        QCOMPARE(display.m_contextPhase, AuthenticationPhase::LoserStopping);
        QVERIFY(!display.m_fingerprintDeadline && !display.m_fingerprintAwaitingAuthentication);
        QTest::qWait(1100);
        QVERIFY(!client.state(FingerprintState::TimedOut));
        QVERIFY(!display.m_contextCommitted);
        display.command(AuthMethod::Password, "E");
        QTRY_VERIFY(display.peers.value(int(AuthMethod::Fingerprint)).authorized);
        QCOMPARE(display.m_contextPhase, AuthenticationPhase::SessionAuthorized);
        clear(client, 2);
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("session_committed")), 1);
        QVERIFY(display.m_contextCommitted);
        QTest::qWait(1100);
        QVERIFY(!client.state(FingerprintState::TimedOut));
        display.command(AuthMethod::Fingerprint, "A");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationAttemptFinished), 1);
        QVERIFY(display.m_contextFinished && !display.m_contextCommitted);
        QVERIFY(client.state(FingerprintState::Matched));
        QVERIFY(client.state(FingerprintState::Failed));
        clear(client, 3);
        QTRY_VERIFY(display.m_pendingContext);
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
    }
    void passwordWinnerNeverMatchesFingerprintAndKeepsCommit() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        display.command(AuthMethod::Password, "S");
        QTRY_VERIFY(display.m_waitingForLoser);
        QVERIFY(!client.state(FingerprintState::Matched));
        QVERIFY(!client.state(FingerprintState::Cancelled));
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(display.peers.value(int(AuthMethod::Password)).authorized);
        QTRY_VERIFY(client.state(FingerprintState::Cancelled));
        QVERIFY(!client.state(FingerprintState::Matched));
        const auto request = display.m_requestId;
        display.finishMethodAttempt(true); // Exercise final binding without creating a user session.
        clear(client, 2);
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("session_committed")), 1);
        QCOMPARE(display.m_requestId, request);
        password(client, QStringLiteral("a"));
        QTest::qWait(20);
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        display.command(AuthMethod::Password, "E");
        QTRY_VERIFY(!display.m_auth->isActive());
        QVERIFY(display.m_contextCommitted);
    }
    void stockCommittedClearRetainsLegacyCompletion() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        connectClient(display, client); // Query alone does not opt into correlated events.
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        display.command(AuthMethod::Password, "S");
        QTRY_VERIFY(display.peers.value(int(AuthMethod::Password)).authorized);
        QVERIFY(!display.m_contextOptedIn);
        clear(client, 1);
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("session_committed")), 1);
        QVERIFY(!display.m_socketServer->isCorrelated(display.m_contextSocket));
        display.command(AuthMethod::Password, "A");
        QTRY_COMPARE(client.count(DaemonMessages::LoginFailed), 1);
        QCOMPARE(client.count(DaemonMessages::AuthenticationAttemptFinished), 0);
        clear(client, 2);
        QTRY_VERIFY(display.m_pendingContext);
        display.command(AuthMethod::Password, "E");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
    }
    void passwordFailureAndRetryLeavesFingerprintActive() {
        mainConfig.FingerprintAuthTimeout.set(30);
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        const auto fingerprintGeneration = display.m_fingerprintAuth->launchGeneration();
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        bool retried = false;
        client.consume = [&](const Event &event) {
            if (!retried && event.message == DaemonMessages::AuthenticationMethodState
                && event.method == quint32(AuthenticationMethod::Password)
                && event.state == quint32(FingerprintState::Failed)) {
                retried = true;
                password(client, QStringLiteral("a"), QStringLiteral("superseded-private-response"));
                password(client, QStringLiteral("a"), QStringLiteral("latest-private-response"));
            }
        };
        display.command(AuthMethod::Password, "F");
        QTRY_VERIFY(display.m_pendingLogin);
        QCOMPARE(display.m_pendingPasswordGeneration, quint64(1));
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        QVERIFY(display.m_auth->isActive());
        QTRY_VERIFY(client.state(FingerprintState::Failed, AuthenticationMethod::Password));
        QVERIFY(display.m_fingerprintAuth->isActive());
        QCOMPARE(client.count(DaemonMessages::AuthenticationAttemptFinished), 0);
        display.command(AuthMethod::Password, "E");
        QTRY_COMPARE(display.m_auth->launchGeneration(), quint64(2));
        QTRY_VERIFY(display.peers.value(int(AuthMethod::Password)).socket
                    && display.peers.value(int(AuthMethod::Password)).socket->state() == QLocalSocket::ConnectedState);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), fingerprintGeneration);
        QVERIFY(!display.m_attemptGate.cancelled(AuthMethod::Password));
        QCOMPARE(display.m_passPhrase, QStringLiteral("latest-private-response"));
        QVERIFY(!display.m_pendingLogin);
        password(client, QStringLiteral("a"), QStringLiteral("nonterminal-duplicate-response"));
        QTest::qWait(20);
        QVERIFY(!display.m_pendingLogin);
        QCOMPARE(display.m_auth->launchGeneration(), quint64(2));
        QCOMPARE(display.m_passPhrase, QStringLiteral("latest-private-response"));
        clear(client, 2);
        QTRY_VERIFY(display.m_pendingContext);
        display.command(AuthMethod::Password, "E");
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
    }
    void fingerprintFailureImmediateRetryLeavesPasswordActive() {
        mainConfig.FingerprintAuthTimeout.set(30);
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        const auto request = display.m_requestId;
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        const auto passwordGeneration = display.m_auth->launchGeneration();
        const auto initialProbes = display.probes;
        bool retried = false;
        client.consume = [&](const Event &event) {
            if (!retried && event.message == DaemonMessages::AuthenticationMethodState
                && event.method == quint32(AuthenticationMethod::Fingerprint)
                && event.state == quint32(FingerprintState::Failed)) {
                retried = true;
                start(client, 1);
                start(client, 1);
            }
        };
        display.command(AuthMethod::Fingerprint, "T");
        QTRY_COMPARE(display.m_pendingFingerprintRetryGeneration, quint64(1));
        QVERIFY(display.m_fingerprintAuth->isActive());
        QCOMPARE(display.probes, initialProbes);
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(1));
        QVERIFY(display.m_auth->isActive());
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_COMPARE(display.probes, initialProbes + 1);
        QVERIFY(!display.m_fingerprintAuth->isActive());
        QCOMPARE(display.m_pendingFingerprintRetryGeneration, quint64(0));
        QCOMPARE(display.m_fingerprintAvailability, AuthenticationAvailability::Unknown);
        QVERIFY(display.m_fingerprintStartRequested);
        display.setFingerprintAvailability(1, request, true, QStringLiteral("available"));
        QTRY_COMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        QTRY_COMPARE(display.peers.value(int(AuthMethod::Fingerprint)).request, request);
        QCOMPARE(display.m_auth->launchGeneration(), passwordGeneration);
        QCOMPARE(display.m_requestId, request);
        QCOMPARE(display.m_contextSerial, quint64(1));
        QVERIFY(!display.m_attemptGate.cancelled(AuthMethod::Fingerprint));
        start(client, 1);
        QTest::qWait(20);
        QCOMPARE(display.m_pendingFingerprintRetryGeneration, quint64(0));
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(2));
        clear(client, 2);
        QTRY_VERIFY(display.m_pendingContext);
        display.command(AuthMethod::Password, "E");
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
    }
    void queuedTerminalRetriesAreInvalidated_data() {
        QTest::addColumn<bool>("fingerprint");
        QTest::addColumn<QString>("action");
        for (bool fingerprint : {false, true})
            for (const QString &action : {QStringLiteral("clear"), QStringLiteral("replace"),
                                         QStringLiteral("disconnect"), QStringLiteral("winner")})
                QTest::newRow(qPrintable((fingerprint ? QStringLiteral("fingerprint-") : QStringLiteral("password-")) + action))
                        << fingerprint << action;
    }
    void queuedTerminalRetriesAreInvalidated() {
        QFETCH(bool, fingerprint);
        QFETCH(QString, action);
        mainConfig.FingerprintAuthTimeout.set(30);
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        const auto retryMethod = fingerprint ? AuthMethod::Fingerprint : AuthMethod::Password;
        const auto otherMethod = fingerprint ? AuthMethod::Password : AuthMethod::Fingerprint;
        display.command(retryMethod, fingerprint ? "T" : "F");
        QTRY_VERIFY(client.state(FingerprintState::Failed, AuthenticationMethod(retryMethod)));
        if (fingerprint) {
            start(client, 1);
            QTRY_COMPARE(display.m_pendingFingerprintRetryGeneration, quint64(1));
        } else {
            password(client, QStringLiteral("a"));
            QTRY_COMPARE(display.m_pendingPasswordGeneration, quint64(1));
        }
        if (action == QLatin1String("clear")) {
            clear(client, 2);
            QTRY_VERIFY(display.m_pendingContext);
        } else if (action == QLatin1String("replace")) {
            select(client, 2, QStringLiteral("b"));
            QTRY_VERIFY(display.m_pendingContext);
        } else if (action == QLatin1String("disconnect")) {
            client.socket.disconnectFromServer();
            QTRY_VERIFY(!display.m_methodAttemptActive);
        } else {
            display.command(otherMethod, "S");
            QTRY_VERIFY(display.m_methodWinnerChosen);
        }
        QCOMPARE(display.m_pendingPasswordGeneration, quint64(0));
        QCOMPARE(display.m_pendingFingerprintRetryGeneration, quint64(0));
        QVERIFY(!display.m_pendingLogin);
        QVERIFY(!display.m_fingerprintStartRequested);
        display.command(retryMethod, "SE"); // Late success from the failed generation is ineligible.
        QTRY_VERIFY(!(fingerprint ? display.m_fingerprintAuth : display.m_auth)->isActive());
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(1));
        if (action == QLatin1String("winner")) {
            QTRY_VERIFY(display.peers.value(int(otherMethod)).authorized);
            display.command(otherMethod, "A");
            QTRY_VERIFY(display.m_contextFinished);
        } else {
            QVERIFY(!display.m_contextCommitted);
        }
        display.command(otherMethod, "E");
        QTRY_VERIFY(!display.m_auth->isActive() && !display.m_fingerprintAuth->isActive());
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        QCOMPARE(display.m_fingerprintAuth->launchGeneration(), quint64(1));
        if (action == QLatin1String("clear"))
            QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
        if (action == QLatin1String("replace")) {
            QCOMPARE(display.m_contextUser, QStringLiteral("b"));
            QCOMPARE(display.m_contextSerial, quint64(2));
        }
    }
    void realProxyCompletionBeforeFinishedRejection_data() {
        QTest::addColumn<bool>("correlated");
        QTest::addColumn<bool>("clearContext");
        QTest::newRow("correlated-clear") << true << true;
        QTest::newRow("correlated-selection") << true << false;
        QTest::newRow("legacy-clear") << false << true;
    }
    void realProxyCompletionBeforeFinishedRejection() {
        QFETCH(bool, correlated);
        QFETCH(bool, clearContext);
        mainConfig.Wayland.SessionDir.set(QStringList{});
        SessionModel model;
        QCOMPARE(model.rowCount(), 1);
        Harness display(directory->filePath(QStringLiteral("control")));
        QVERIFY(display.m_socketServer->start(QStringLiteral("daemon-proxy-context-test")));
        // The daemon application's stock hostname/power providers are absent
        // from this unprivileged fixture. Strip only the payload-free Connect;
        // every method query/context/auth/result uses both production peers.
        QLocalServer transport;
        QVERIFY(transport.listen(directory->filePath(QStringLiteral("proxy-transport"))));
        QLocalSocket daemonSocket;
        daemonSocket.connectToServer(display.m_socketServer->socketAddress());
        QTRY_COMPARE(daemonSocket.state(), QLocalSocket::ConnectedState);
        QPointer<QLocalSocket> proxySocket;
        QByteArray initialBytes;
        bool connectConsumed = false;
        bool badConnect = false;
        QObject::connect(&transport, &QLocalServer::newConnection, &transport, [&] {
            proxySocket = transport.nextPendingConnection();
            QObject::connect(proxySocket, &QLocalSocket::readyRead, &transport, [&] {
                QByteArray bytes = proxySocket->readAll();
                if (!connectConsumed) {
                    initialBytes.append(bytes);
                    if (initialBytes.size() < int(sizeof(quint32)))
                        return;
                    QDataStream input(initialBytes);
                    quint32 message = 0;
                    input >> message;
                    badConnect = message != quint32(GreeterMessages::Connect);
                    connectConsumed = true;
                    bytes = initialBytes.mid(sizeof(quint32));
                    initialBytes.clear();
                }
                daemonSocket.write(bytes);
                daemonSocket.flush();
            });
        });
        QObject::connect(&daemonSocket, &QLocalSocket::readyRead, &transport, [&] {
            if (proxySocket) {
                proxySocket->write(daemonSocket.readAll());
                proxySocket->flush();
            }
        });
        GreeterProxy proxy(transport.fullServerName());
        proxy.setSessionModel(&model);
        proxy.setAuthenticationController(true);
        QTRY_VERIFY(proxy.authenticationMethodsAvailable());
        QVERIFY(connectConsumed && !badConnect);
        if (correlated) {
            proxy.setAuthenticationContext(QStringLiteral("a"), 0);
            QTRY_VERIFY(!proxy.fingerprintRequestId().isEmpty());
        }
        proxy.login(QStringLiteral("a"), QStringLiteral("private-test-response"), 0);
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        const auto originalRequest = display.m_requestId;
        const auto originalSerial = display.m_contextSerial;
        // Hold only delivery of the real proxy's replacement request. The
        // committed helper and daemon final-result transport continue normally.
        QObject::disconnect(display.m_socketServer, &SocketServer::setAuthenticationContext,
                            &display, &Display::setAuthenticationContext);
        QPointer<QLocalSocket> pendingSocket;
        quint64 pendingSerial = 0;
        bool pendingHasContext = false;
        QString pendingUser;
        Session pendingSession;
        QObject::connect(display.m_socketServer, &SocketServer::setAuthenticationContext, &display,
                         [&](QLocalSocket *socket, quint64 serial, bool hasContext,
                             const QString &user, const Session &selectedSession) {
            pendingSocket = socket;
            pendingSerial = serial;
            pendingHasContext = hasContext;
            pendingUser = user;
            pendingSession = selectedSession;
        });
        QSignalSpy success(&proxy, &GreeterProxy::loginSucceeded);
        QSignalSpy failure(&proxy, &GreeterProxy::loginFailed);
        QSignalSpy completion(&proxy, &GreeterProxy::authenticationFinished);
        QSignalSpy rejection(&proxy, &GreeterProxy::authenticationContextRejected);
        QSignalSpy cleared(&proxy, &GreeterProxy::authenticationContextCleared);
        if (clearContext)
            proxy.clearAuthenticationContext();
        else
            proxy.setAuthenticationContext(QStringLiteral("b"), 0);
        QTRY_VERIFY(pendingSerial != 0);
        display.command(AuthMethod::Password, "S");
        QTRY_VERIFY(display.peers.value(int(AuthMethod::Password)).authorized);
        QVERIFY(display.m_contextCommitted);
        // Publish the same final-success transition used by session completion,
        // without establishing a user session or writing daemon stateConfig.
        emit display.loginSucceeded(display.m_attemptSocket);
        display.finishMethodAttempt(true);
        QCOMPARE(display.m_contextPhase, AuthenticationPhase::Finished);
        display.setAuthenticationContext(pendingSocket, pendingSerial, pendingHasContext,
                                         pendingUser, pendingSession);
        QTRY_COMPARE(success.size(), 1);
        QTRY_COMPARE(rejection.size(), 1);
        QCOMPARE(failure.size(), 0);
        QCOMPARE(cleared.size(), 0);
        QCOMPARE(completion.size(), correlated ? 1 : 0);
        QCOMPARE(proxy.authenticationUser(), QStringLiteral("a"));
        QCOMPARE(proxy.authenticationSessionIndex(), 0);
        QVERIFY(proxy.authenticationBusy());
        QVERIFY(!proxy.authenticationSubmissionAllowed());
        QCOMPARE(proxy.fingerprintRequestId(), correlated ? originalRequest : QString());
        QCOMPARE(display.m_requestId, originalRequest);
        QCOMPARE(display.m_contextSerial, originalSerial);
        QVERIFY(display.m_contextCommitted && display.m_contextFinished == correlated);
        proxy.login(QStringLiteral("b"), QStringLiteral("blocked-private-response"), 0);
        QTest::qWait(20);
        QCOMPARE(display.m_auth->launchGeneration(), quint64(1));
        display.command(AuthMethod::Password, "E");
        QTRY_VERIFY(!display.m_auth->isActive());
        QCOMPARE(success.size(), 1);
        QVERIFY(display.m_contextCommitted);
    }
    void selectedWinnerClearDeniesBeforeLoserExit() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        password(client, QStringLiteral("a"));
        QTRY_VERIFY(display.peers.contains(int(AuthMethod::Password)));
        display.command(AuthMethod::Fingerprint, "S");
        QTRY_VERIFY(display.m_waitingForLoser);
        clear(client, 2);
        QTRY_VERIFY(display.m_pendingContext);
        QVERIFY(!display.m_methodAttemptActive && !display.m_contextCommitted);
        display.command(AuthMethod::Password, "E");
        display.command(AuthMethod::Fingerprint, "SE");
        QTRY_COMPARE(client.count(DaemonMessages::AuthenticationContext, QStringLiteral("cleared")), 1);
        for (const auto &event : client.events)
            QVERIFY(event.phase != quint32(AuthenticationPhase::SessionAuthorized));
        QVERIFY(!display.m_contextCommitted);
    }
    void exitedHelperCannotBecomeAuthorized() {
        Harness display(directory->filePath(QStringLiteral("control")));
        Client client;
        activate(display, client, QStringLiteral("a"));
        display.command(AuthMethod::Fingerprint, "E");
        QTRY_VERIFY(!display.m_fingerprintAuth->isActive());
        // Deliberately dispatch a delayed success notification after observed
        // exit. The production callback must not select a dead helper.
        emit display.m_fingerprintAuth->authentication(QStringLiteral("a"), true);
        QVERIFY(!display.m_methodWinnerChosen && !display.m_contextCommitted);
        QVERIFY(!client.state(FingerprintState::Matched));
    }
};
}
QTEST_GUILESS_MAIN(SDDM::DaemonContextTest)
#include "DaemonContextTest.moc"
