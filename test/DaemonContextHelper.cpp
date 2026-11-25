// Unprivileged deterministic transport peer; never invokes PAM or starts a session.
#include "Auth.h"
#include "AuthMessages.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>
#include <QTimer>
#include <csignal>
#include <cstring>

using namespace SDDM;

static QByteArray frame(const QByteArray &payload) {
    QByteArray result;
    const qint64 size = payload.size();
    result.append(reinterpret_cast<const char *>(&size), sizeof(size));
    result.append(payload);
    return result;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    auto argument = [&args](const QString &name) {
        const int index = args.indexOf(name);
        return index >= 0 && index + 1 < args.size() ? args.at(index + 1) : QString();
    };
    const QString request = argument(QStringLiteral("--request-id"));
    const QString user = argument(QStringLiteral("--user"));
    const auto method = AuthMethod(argument(QStringLiteral("--method")).toInt());
    if (argument(QStringLiteral("--start")) == QLatin1String("ignore-term"))
        std::signal(SIGTERM, SIG_IGN);
    QLocalSocket auth, control;
    QByteArray incoming;
    auto send = [&](Msg message, const auto &append) {
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << message << request << method;
        append(stream);
        auth.write(frame(payload));
        auth.flush();
    };
    auto progress = [&](FingerprintState state, const QString &detail) {
        send(METHOD_STATE, [&](QDataStream &stream) { stream << user << quint32(state) << detail; });
    };
    QObject::connect(&auth, &QLocalSocket::connected, &app, [&] {
        QByteArray hello;
        QDataStream stream(&hello, QIODevice::WriteOnly);
        stream << HELLO << argument(QStringLiteral("--id")).toLongLong() << request << method;
        auth.write(frame(hello));
        if (method == AuthMethod::Fingerprint)
            progress(FingerprintState::Scanning, QStringLiteral("scanning"));
        control.connectToServer(argument(QStringLiteral("--display-server")));
    });
    QObject::connect(&control, &QLocalSocket::connected, &app, [&] {
        control.write("READY " + QByteArray::number(int(method)) + ' ' + request.toUtf8() + '\n');
        control.flush();
    });
    QObject::connect(&control, &QLocalSocket::readyRead, &app, [&] {
        const auto commands = control.readAll();
        for (char command : commands) {
            switch (command) {
            case 'S':
                send(AUTHENTICATED, [&](QDataStream &stream) { stream << user; });
                break;
            case 'U':
                progress(FingerprintState::Unavailable, QStringLiteral("authentication_unavailable"));
                send(AUTHENTICATED, [](QDataStream &stream) { stream << QString(); });
                break;
            case 'R':
                progress(FingerprintState::Retry, QStringLiteral("try_again"));
                break;
            case 'I':
                send(INFO, [](QDataStream &stream) { stream << QStringLiteral("obsolete private feedback") << Auth::INFO_UNKNOWN; });
                send(ERROR, [](QDataStream &stream) { stream << QStringLiteral("obsolete private error") << Auth::ERROR_AUTHENTICATION; });
                break;
            case 'T':
                progress(FingerprintState::Failed, QStringLiteral("authentication_failed"));
                send(AUTHENTICATED, [](QDataStream &stream) { stream << QString(); });
                break;
            case 'F':
                send(AUTHENTICATED, [](QDataStream &stream) { stream << QString(); });
                break;
            case 'A':
                send(SESSION_STATUS, [](QDataStream &stream) { stream << false; });
                break;
            case 'E':
                app.exit(Auth::HELPER_AUTH_ERROR);
                break;
            default:
                app.exit(10);
                break;
            }
        }
    });
    QObject::connect(&auth, &QLocalSocket::readyRead, &app, [&] {
        incoming.append(auth.readAll());
        while (incoming.size() >= qsizetype(sizeof(qint64))) {
            qint64 length = 0;
            std::memcpy(&length, incoming.constData(), sizeof(length));
            if (length < 0 || length > 1024 * 1024) { app.exit(11); return; }
            if (incoming.size() < qint64(sizeof(length)) + length)
                return;
            QDataStream stream(incoming.mid(sizeof(length), length));
            Msg message = MSG_UNKNOWN;
            QString id;
            AuthMethod responseMethod = AuthMethod::Password;
            stream >> message >> id >> responseMethod;
            if (stream.status() != QDataStream::Ok || id != request || responseMethod != method) {
                app.exit(12);
                return;
            }
            if (message == SESSION_AUTHORIZATION) {
                bool authorized = false;
                stream >> authorized;
                control.write(authorized ? "AUTHORIZED\n" : "DENIED\n");
                control.flush();
            } else if (message != SESSION_STATUS) {
                app.exit(13);
                return;
            }
            incoming.remove(0, sizeof(length) + length);
        }
    });
    QTimer::singleShot(15000, &app, [&] { app.exit(14); });
    auth.connectToServer(argument(QStringLiteral("--socket")));
    return app.exec();
}
