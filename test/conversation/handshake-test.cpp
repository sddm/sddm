#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <QUuid>
#include "AuthMessages.h"
#include "SafeDataStream.h"
#include "Constants.h"
using namespace SDDM;

struct HandshakeOwner {
    int errors = 0, stops = 0;
    void error(const QString &, Auth::Error) { ++errors; }
    void stop() { ++stops; }
};
struct FixtureChild {
    QProcess::ProcessState current = QProcess::Running;
    QStringList arguments;
    QProcess::ProcessState state() const { return current; }
    void start(const QString &, const QStringList &args) { arguments = args; current = QProcess::Running; }
};
struct HandshakeHelper {
    bool cancelled = false, conversation = false;
    FixtureChild storage;
    FixtureChild *child = &storage;
    HandshakeOwner ownerStorage;
    HandshakeOwner *owner = &ownerStorage;
    QLocalSocket *socket = nullptr;
    int pending = 0;
    void setSocket(QLocalSocket *value) { socket = value; }
    void dataPending() { ++pending; }
};
class HandshakeServer : public QLocalServer {
public:
    QMap<qint64, HandshakeHelper *> helpers;
    void handleNewConnection();
};
#include "HandshakeMethods.inc"

struct FixtureRegistry {
    QMap<qint64, void *> helpers;
    static qint64 lastId;
    static FixtureRegistry *instance() { static FixtureRegistry registry; return &registry; }
    QString fullServerName() const { return QStringLiteral("synthetic-server"); }
};
qint64 FixtureRegistry::lastId = 10;
struct StartRequest { void setRequest() {} };
struct StartData : public QObject {
    qint64 id = 1;
    QLocalSocket *socket = nullptr;
    StartRequest requestStorage;
    StartRequest *request = &requestStorage;
    FixtureChild childStorage;
    FixtureChild *child = &childStorage;
    QTimer timer;
    QTimer *killTimer = &timer;
    bool cancelled = true, authResultSeen = true, authenticationSucceeded = true;
    bool conversation = true, autologin = false, greeter = false;
    QString sessionPath, user = QStringLiteral("alice"), displayServerCmd;
};
class StartFixture {
public:
    StartData data;
    StartData *d = &data;
    bool isActive() const { return d->child->state() != QProcess::NotRunning; }
    void start();
};
#include "StartMethod.inc"

class HandshakeTest : public QObject {
    Q_OBJECT
private slots:
    void startRetiresPreviousIdentityAndSocket() {
        StartFixture auth;
        auto *registry = FixtureRegistry::instance();
        registry->helpers.clear();
        registry->helpers[auth.d->id] = auth.d;
        QPointer<QLocalSocket> previous = new QLocalSocket;
        auth.d->socket = previous;
        auth.d->child->current = QProcess::NotRunning;
        auth.start();
        QVERIFY(!registry->helpers.contains(1));
        QCOMPARE(registry->helpers.value(auth.d->id), static_cast<void *>(auth.d));
        QVERIFY(auth.d->id != 1);
        QVERIFY(!auth.d->socket);
        QVERIFY(!auth.d->cancelled);
        const auto args = auth.d->child->arguments;
        QCOMPARE(args.value(args.indexOf(QStringLiteral("--id")) + 1), QString::number(auth.d->id));
        QVERIFY(args.contains(QStringLiteral("--conversation")));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(previous.isNull());
    }
    void modesAndIdentity_data() {
        QTest::addColumn<bool>("conversation"); QTest::addColumn<int>("opcode");
        QTest::addColumn<int>("version"); QTest::addColumn<bool>("duplicate");
        QTest::addColumn<bool>("stale"); QTest::addColumn<bool>("accepted");
        QTest::newRow("legacy") << false << int(HELLO) << 0 << false << false << true;
        QTest::newRow("conversation") << true << int(HELLO_CONVERSATION) << 1 << false << false << true;
        QTest::newRow("wrong-version") << true << int(HELLO_CONVERSATION) << 2 << false << false << false;
        QTest::newRow("legacy-in-conversation") << true << int(HELLO) << 0 << false << false << false;
        QTest::newRow("conversation-in-legacy") << false << int(HELLO_CONVERSATION) << 1 << false << false << false;
        QTest::newRow("duplicate") << true << int(HELLO_CONVERSATION) << 1 << true << false << false;
        QTest::newRow("retired-id") << true << int(HELLO_CONVERSATION) << 1 << false << true << false;
    }
    void modesAndIdentity() {
        QFETCH(bool, conversation); QFETCH(int, opcode); QFETCH(int, version);
        QFETCH(bool, duplicate); QFETCH(bool, stale); QFETCH(bool, accepted);
        HandshakeServer server;
        QVERIFY(server.listen(QStringLiteral("sddm-handshake-") + QUuid::createUuid().toString(QUuid::WithoutBraces)));
        HandshakeHelper helper; helper.conversation = conversation;
        QLocalSocket existing;
        if (duplicate) helper.socket = &existing;
        server.helpers[2] = &helper; // Only the new helper identity remains registered.
        QLocalSocket client;
        client.connectToServer(server.fullServerName()); QVERIFY(client.waitForConnected());
        SafeDataStream output(&client);
        output << Msg(opcode) << qint64(stale ? 1 : 2);
        if (opcode == HELLO_CONVERSATION) output << quint32(version);
        output.send(); QCOMPARE(output.status(), QDataStream::Ok);
        QVERIFY(server.waitForNewConnection());
        server.handleNewConnection();
        QCOMPARE(helper.socket != nullptr && helper.socket != &existing, accepted);
        QCOMPARE(helper.owner->stops, accepted || stale ? 0 : 1);
        QCOMPARE(helper.owner->errors, accepted || stale ? 0 : 1);
    }
};
QTEST_GUILESS_MAIN(HandshakeTest)
#include "handshake-test.moc"
