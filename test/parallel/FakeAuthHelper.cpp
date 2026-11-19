// SPDX-License-Identifier: GPL-2.0-or-later

// Test-only protocol peer. Never installed; performs no PAM checks or sessions.
#include "AuthMessages.h"
#include "SafeDataStream.h"
#include <QCoreApplication>
#include <QFile>
#include <QLocalSocket>
#include <QTimer>
#include <csignal>
#include <unistd.h>
using namespace SDDM;
int main(int argc, char **argv) {
    if (argc == 2 && QByteArray(argv[1]) == "--parallel-probe-hang") { sleep(5); return 1; }
    if (argc == 2 && QByteArray(argv[1]) == "--parallel-probe-slow") {
        usleep(300000);
        if (write(STDOUT_FILENO, "READY", 5) != 5) return 1;
        char reply = 0;
        return read(STDIN_FILENO, &reply, 1) == 1 && reply == 'x' ? 0 : 1;
    }
    if (argc == 2 && QByteArray(argv[1]) == "--parallel-probe") {
        if (write(STDOUT_FILENO, "READY", 5) != 5) return 1;
        char reply = 0;
        return read(STDIN_FILENO, &reply, 1) == 1 && reply == 'x' ? 0 : 1;
    }
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    if (args.contains(QStringLiteral("--test-exit-before-hello"))) return 90;
    auto arg = [&](const QString &key) {
        int i = args.indexOf(key);
        return i >= 0 ? args.value(i + 1) : QString();
    };
    auto fields = arg(QStringLiteral("--start")).split(QLatin1Char(';'));
    if (fields.size() < 3)
        return 90;
    int delay = fields[0].toInt();
    QString mode = fields[1], proof = fields[2];
    if (fields[0] == QStringLiteral("display-fixture") && fields.size() == 5) {
        const bool fp = args.contains(QStringLiteral("--fingerprint"));
        delay = fields[fp ? 2 : 1].toInt();
        const bool failPassword = fields[3] == QStringLiteral("password-fail") ||
                                  fields[3] == QStringLiteral("password-fail-disconnect");
        mode = fp ? (failPassword ? QStringLiteral("ok") : fields[3]) :
                    (fields[3] == QStringLiteral("password-fail-disconnect") ? QStringLiteral("fail-disconnect") :
                     failPassword ? QStringLiteral("fail") : QStringLiteral("ok"));
        proof = fields[4] + (fp ? QStringLiteral("-fp") : QStringLiteral("-pw"));
    }
    if (mode == QStringLiteral("ignore-term"))
        std::signal(SIGTERM, SIG_IGN);
    const bool parallel = args.contains(QStringLiteral("--parallel-auth-v2"));
    if (mode == QStringLiteral("no-hello")) {
        QTimer::singleShot(9000, &app, [&] { app.exit(91); });
        return app.exec();
    }
    QLocalSocket socket;
    QObject::connect(&socket, &QLocalSocket::connected, &app, [&] {
        if (mode == QStringLiteral("partial-length") || mode == QStringLiteral("partial-payload") ||
            mode == QStringLiteral("oversized") || mode == QStringLiteral("disconnect")) {
            qint64 length = mode == QStringLiteral("oversized") ? 1024 * 1024 : 12;
            socket.write(reinterpret_cast<const char *>(&length), mode == QStringLiteral("partial-length") ? 1 : sizeof(length));
            if (mode == QStringLiteral("partial-payload")) socket.write("\0", 1);
            socket.flush();
            if (mode == QStringLiteral("disconnect")) socket.disconnectFromServer();
            return;
        }
        SafeDataStream stream(&socket);
        stream << (parallel && mode != QStringLiteral("legacy") ? HELLO_V2 : HELLO) << arg(QStringLiteral("--id")).toLongLong();
        stream.send();
        if (mode.startsWith(QStringLiteral("post-"))) {
            qint64 length = mode == QStringLiteral("post-oversized") ? 1024 * 1024 + 1 : 12;
            socket.write(reinterpret_cast<const char *>(&length), mode == QStringLiteral("post-partial-length") ? 1 : sizeof(length));
            if (mode == QStringLiteral("post-partial-payload")) socket.write("\0", 1);
            socket.flush();
            if (mode == QStringLiteral("post-disconnect")) socket.disconnectFromServer();
            return;
        }
        QTimer::singleShot(delay, &app, [&] {
            if (mode == QStringLiteral("crash")) {
                std::raise(SIGKILL);
                return;
            }
            SafeDataStream out(&socket);
            if (mode == QStringLiteral("prompt")) {
                Request request;
                request.prompts << Prompt(AuthPrompt::LOGIN_PASSWORD, QStringLiteral("Password:"), true);
                out << REQUEST << request;
                out.send();
                out.receive();
                app.exit(1);
                return;
            }
            const bool failed = mode == QStringLiteral("fail") || mode.startsWith(QStringLiteral("fail-"));
            out << AUTHENTICATED << (failed ? QString() : arg(QStringLiteral("--user")));
            out.send();
            if (failed) {
                if (parallel && !mode.startsWith(QStringLiteral("fail-disconnect"))) {
                    out.receive();
                    Msg ack = MSG_UNKNOWN;
                    out >> ack;
                    if (ack != AUTHENTICATED) return;
                }
                if (mode == QStringLiteral("fail-then-success")) {
                    QTimer::singleShot(50, &app, [&] {
                        SafeDataStream late(&socket);
                        late << AUTHENTICATED << arg(QStringLiteral("--user"));
                        late.send();
                        late.receive();
                        Msg msg = MSG_UNKNOWN;
                        bool approved = false;
                        late >> msg;
                        if (parallel) late >> approved;
                        else approved = msg == AUTHENTICATED;
                        if (approved) {
                            QFile f(proof);
                            if (f.open(QIODevice::WriteOnly)) f.write("approved");
                        }
                        app.exit(1);
                    });
                    return;
                }
                if (mode.startsWith(QStringLiteral("fail-disconnect"))) {
                    if (mode == QStringLiteral("fail-disconnect-partial")) {
                        socket.write("x", 1);
                        socket.flush();
                    }
                    socket.disconnectFromServer();
                }
                if (mode.endsWith(QStringLiteral("crash")))
                    QTimer::singleShot(50, &app, [] { std::raise(SIGKILL); });
                else if (mode.endsWith(QStringLiteral("hang")))
                    QTimer::singleShot(9000, &app, [&] { app.exit(1); });
                else if (mode == QStringLiteral("fail-disconnect-success-exit"))
                    QTimer::singleShot(100, &app, [&] { app.exit(0); });
                else if (mode == QStringLiteral("fail-disconnect") || mode == QStringLiteral("fail-disconnect-partial"))
                    QTimer::singleShot(500, &app, [&] { app.exit(1); });
                else
                    app.exit(1);
                return;
            }
            out.receive();
            Msg msg = MSG_UNKNOWN;
            bool approved = false;
            QProcessEnvironment env;
            QByteArray cookie;
            out >> msg;
            if (parallel && mode != QStringLiteral("legacy"))
                out >> approved;
            else
                approved = true;
            out >> env >> cookie;
            approved = approved && out.status() == QDataStream::Ok;
            QFile result(proof);
            result.open(QIODevice::WriteOnly);
            result.write(approved && msg == AUTHENTICATED ? "approved" : "rejected");
            result.close();
            QTimer::singleShot(approved ? 2000 : 0, &app, [&] { app.exit(approved ? 0 : 1); });
        });
    });
    socket.connectToServer(arg(QStringLiteral("--socket")));
    QTimer::singleShot(6000, &app, [&] { app.exit(91); });
    return app.exec();
}
