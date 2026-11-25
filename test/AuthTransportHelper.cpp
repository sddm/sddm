// An unprivileged wire peer, NOT a PAM helper or a session launcher.
#include "AuthMessages.h"

#include <QCoreApplication>
#include <QProcess>
#include <QDataStream>
#include <QLocalSocket>
#include <QTimer>

using namespace SDDM;

static QByteArray frame(const QByteArray &payload) {
    QByteArray bytes;
    const qint64 size = payload.size();
    bytes.append(reinterpret_cast<const char *>(&size), sizeof(size));
    bytes.append(payload);
    return bytes;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    auto value = [&args](const QString &flag) {
        const int index = args.indexOf(flag);
        return index < 0 || index + 1 == args.size() ? QString() : args.at(index + 1);
    };
    const QString user = value(QStringLiteral("--user"));
    const QString id = value(QStringLiteral("--request-id"));
    const auto method = AuthMethod(value(QStringLiteral("--method")).toInt());
    if (args.contains(QStringLiteral("--forger"))) {
        QLocalSocket forged;
        forged.connectToServer(value(QStringLiteral("--socket")));
        if (!forged.waitForConnected(1000))
            return 4;
        QByteArray hello;
        QDataStream stream(&hello, QIODevice::WriteOnly);
        stream << HELLO << value(QStringLiteral("--id")).toLongLong() << id << method;
        forged.write(frame(hello));
        forged.flush();
        return forged.waitForDisconnected(1000) ? 0 : 5;
    }
    QLocalSocket socket;
    QObject::connect(&socket, &QLocalSocket::connected, &app, [&] {
        QByteArray hello, authenticated;
        QDataStream first(&hello, QIODevice::WriteOnly);
        if (user == QStringLiteral("forged")) {
            QProcess forger;
            forger.start(QCoreApplication::applicationFilePath(), args.mid(1) << QStringLiteral("--forger"));
            if (!forger.waitForFinished(2000) || forger.exitCode() != 0) {
                app.exit(6);
                return;
            }
        }
        first << HELLO << value(QStringLiteral("--id")).toLongLong() << id << method;
        QDataStream second(&authenticated, QIODevice::WriteOnly);
        second << AUTHENTICATED << (user == QStringLiteral("wrong") ? QStringLiteral("stale") : id)
               << method << user;
        if (user == QStringLiteral("split")) {
            socket.write(frame(hello));
            QTimer::singleShot(30, &app, [&, authenticated] { socket.write(frame(authenticated)); });
        } else if (user == QStringLiteral("fragmented")) {
            const QByteArray bytes = frame(hello);
            socket.write(bytes.left(3));
            QTimer::singleShot(30, &app, [&, bytes, authenticated] {
                socket.write(bytes.mid(3));
                socket.write(frame(authenticated));
            });
        } else {
            socket.write(frame(hello) + frame(authenticated));
        }
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &app, [&] {
        if (user != QStringLiteral("clean-exit"))
            app.quit();
    });
    if (user == QStringLiteral("clean-exit")) {
        QObject::connect(&socket, &QLocalSocket::readyRead, &app, [&] {
            if (socket.bytesAvailable() < qint64(sizeof(qint64)))
                return;
            qint64 length = 0;
            socket.peek(reinterpret_cast<char *>(&length), sizeof(length));
            if (length < 0 || socket.bytesAvailable() < qint64(sizeof(length)) + length)
                return;
            socket.read(reinterpret_cast<char *>(&length), sizeof(length));
            QDataStream response(socket.read(length));
            Msg message = MSG_UNKNOWN;
            QString requestId;
            AuthMethod responseMethod = AuthMethod::Password;
            bool proceed = false;
            response >> message >> requestId >> responseMethod >> proceed;
            if (response.status() != QDataStream::Ok || message != SESSION_AUTHORIZATION
                || requestId != id || responseMethod != method || !proceed) {
                app.exit(7);
                return;
            }
            socket.disconnectFromServer();
            QTimer::singleShot(100, &app, &QCoreApplication::quit);
        });
    }
    QTimer::singleShot(user == QStringLiteral("slow") ? 10000 : 2500, &app, [&] { app.exit(2); });
    socket.connectToServer(value(QStringLiteral("--socket")));
    if (!socket.waitForConnected(1000))
        return 3;
    return app.exec();
}
