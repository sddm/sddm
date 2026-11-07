#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>
#include "SafeDataStream.h"
#include "AuthMessages.h"
using namespace SDDM;
class FrameHelper {
public:
    QLocalSocket *m_socket;
    Request request(const Request &request);
};
#include "HelperRequest.inc"
using namespace SDDM;

class AuthFrameTest : public QObject {
    Q_OBJECT
private:
    QLocalServer server;
    QLocalSocket receiver;
    QLocalSocket *sender = nullptr;
    QByteArray frame(const QString &text) {
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out << text;
        const qint64 size = payload.size();
        QByteArray result(reinterpret_cast<const char *>(&size), sizeof(size));
        result.append(payload);
        return result;
    }
private slots:
    void init() {
        QVERIFY(server.listen(QStringLiteral("conversation-frame-") + QUuid::createUuid().toString(QUuid::WithoutBraces)));
        receiver.connectToServer(server.fullServerName());
        QVERIFY(receiver.waitForConnected());
        QVERIFY(server.waitForNewConnection());
        sender = server.nextPendingConnection();
    }
    void cleanup() { delete sender; sender = nullptr; receiver.abort(); server.close(); }
    void humanReplyMayArriveAfterOneSecond() {
        const auto data = frame(QStringLiteral("synthetic-response"));
        const int descriptor = sender->socketDescriptor();
        std::thread writer([descriptor, data] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            for (int i = 0; i < data.size(); ++i) {
                ::write(descriptor, data.constData() + i, 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        SafeDataStream input(&receiver);
        input.receive(true);
        QString value; input >> value;
        writer.join();
        QCOMPARE(input.status(), QDataStream::Ok);
        QCOMPARE(value, QStringLiteral("synthetic-response"));
    }
    void actualHelperRequestWaitsForHumanReply() {
        const Request prompt({Prompt(AuthPrompt::UNKNOWN, QStringLiteral("Password:"), true)});
        Request reply = prompt;
        reply.prompts[0].response = QByteArray("synthetic-response");
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out << REQUEST << reply;
        const qint64 size = payload.size();
        QByteArray packet(reinterpret_cast<const char *>(&size), sizeof(size));
        packet.append(payload);
        const int descriptor = sender->socketDescriptor();
        std::thread responder([descriptor, packet] {
            // The production helper sends its prompt before waiting for this reply.
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            ::write(descriptor, packet.constData(), packet.size());
        });
        FrameHelper helper{&receiver};
        const Request received = helper.request(prompt);
        responder.join();
        QVERIFY(received.valid());
        QCOMPARE(received.prompts.size(), 1);
        QCOMPARE(received.prompts[0].response, QByteArray("synthetic-response"));
    }
    void incompleteHeaderHasBoundedFailure() {
        const auto data = frame(QStringLiteral("synthetic"));
        sender->write(data.left(3)); sender->flush();
        QElapsedTimer timer; timer.start();
        SafeDataStream input(&receiver); input.receive();
        QCOMPARE(input.status(), QDataStream::ReadPastEnd);
        QVERIFY(timer.elapsed() < 3000);
    }
    void closedPeerInterruptsWaitingForHuman() {
        const int descriptor = sender->socketDescriptor();
        std::thread closer([descriptor] {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ::shutdown(descriptor, SHUT_RDWR);
        });
        SafeDataStream input(&receiver); input.receive(true);
        closer.join();
        QCOMPARE(input.status(), QDataStream::ReadPastEnd);
    }
    void oversizedFrameIsRejectedBeforeAllocation() {
        const qint64 size = 1024 * 1024 + 1;
        sender->write(reinterpret_cast<const char *>(&size), sizeof(size)); sender->flush();
        SafeDataStream input(&receiver); input.receive();
        QCOMPARE(input.status(), QDataStream::ReadCorruptData);
    }
};
QTEST_GUILESS_MAIN(AuthFrameTest)
#include "frame-test.moc"
