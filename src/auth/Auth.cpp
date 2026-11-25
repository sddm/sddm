/*
 * Qt Authentication Library
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include "Auth.h"
#include "Constants.h"
#include "AuthMessages.h"
#include "SafeDataStream.h"

#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <QtQml/QtQml>

#include <memory>

#include <unistd.h>
#include <sys/socket.h>

namespace SDDM {
    class Auth::SocketServer : public QLocalServer {
        Q_OBJECT
    public slots:
        void handleNewConnection();
    public:
        static SocketServer *instance();

        QMap<qint64, Auth::Private*> helpers;
        int pendingHandshakes { 0 };
    private:
        SocketServer();
    };

    class Auth::Private : public QObject {
        Q_OBJECT
    public:
        Private(Auth *parent);
        ~Private();
        void setSocket(QLocalSocket *socket);
        bool acceptHello(QLocalSocket *socket);
    public slots:
        void dataPending();
        void childExited(int exitCode, QProcess::ExitStatus exitStatus);
        void childError(QProcess::ProcessError error);
        void requestFinished();
    public:
        AuthRequest *request { nullptr };
        QProcess *child { nullptr };
        QLocalSocket *socket { nullptr };
        QString displayServerCmd;
        QString sessionPath { };
        QString user { };
        QByteArray cookie { };
        bool autologin { false };
        bool greeter { false };
        bool deferredAuthorization { false };
        bool awaitingAuthorization { false };
        bool authorizationSent { false };
        bool helloAccepted { false };
        bool stopping { false };
        quint64 launchGeneration { 0 };
        AuthMethod method { AuthMethod::Password };
        QString requestId { QUuid::createUuid().toString(QUuid::WithoutBraces) };
        QProcessEnvironment environment { };
        qint64 id { 0 };
        static qint64 lastId;
    };

    qint64 Auth::Private::lastId = 1;



    Auth::SocketServer::SocketServer()
            : QLocalServer() {
        connect(this, &QLocalServer::newConnection, this, &Auth::SocketServer::handleNewConnection);
    }

    void Auth::SocketServer::handleNewConnection()  {
        while (hasPendingConnections()) {
            QLocalSocket *socket = nextPendingConnection();
            socket->setReadBufferSize(1024 * 1024);
            if (++pendingHandshakes > 16) {
                --pendingHandshakes;
                socket->abort();
                socket->deleteLater();
                continue;
            }
            socket->setProperty("sddm-auth-handshake-pending", true);
            QTimer::singleShot(5000, socket, [this, socket] {
                if (!socket->property("sddm-auth-handshake-pending").toBool())
                    return;
                socket->setProperty("sddm-auth-handshake-pending", false);
                --pendingHandshakes;
                socket->abort();
            });
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (socket->bytesAvailable() < qint64(sizeof(qint64)))
                    return;
                qint64 length = -1;
                if (socket->peek(reinterpret_cast<char *>(&length), sizeof(length)) != sizeof(length)
                    || length < 0 || length > 1024 * 1024) {
                    socket->abort();
                    socket->deleteLater();
                    return;
                }
                if (socket->bytesAvailable() < qint64(sizeof(length)) + length)
                    return;
                socket->read(reinterpret_cast<char *>(&length), sizeof(length));
                QByteArray payload = socket->read(length);
                QDataStream str(payload);
                Msg message = Msg::MSG_UNKNOWN;
                qint64 id = 0;
                QString requestId;
                AuthMethod method = AuthMethod::Password;
                str >> message >> id >> requestId >> method;
                if (str.status() != QDataStream::Ok || !str.atEnd() || message != Msg::HELLO || !id
                    || !helpers.contains(id) || !helpers[id]->acceptHello(socket)) {
                    socket->setProperty("sddm-auth-handshake-pending", false);
                    --pendingHandshakes;
                    socket->abort();
                    socket->deleteLater();
                    return;
                }
                Auth::Private *helper = helpers[id];
                if (helper->helloAccepted || helper->requestId != requestId || helper->method != method) {
                    socket->setProperty("sddm-auth-handshake-pending", false);
                    --pendingHandshakes;
                    socket->abort();
                    socket->deleteLater();
                    return;
                }
                helper->helloAccepted = true;
                socket->setProperty("sddm-auth-hello-accepted", true);
                socket->setProperty("sddm-auth-handshake-pending", false);
                --pendingHandshakes;
                QObject::disconnect(socket, &QLocalSocket::readyRead, this, nullptr);
                helper->setSocket(socket);
                helper->dataPending();
            });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
                if (socket->property("sddm-auth-handshake-pending").toBool()) {
                    socket->setProperty("sddm-auth-handshake-pending", false);
                    --pendingHandshakes;
                }
                socket->deleteLater();
            });
        }
    }

    Auth::SocketServer* Auth::SocketServer::instance() {
        static std::unique_ptr<Auth::SocketServer> self;
        if (!self) {
            self.reset(new SocketServer());
            self->listen(QStringLiteral("sddm-auth-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        }
        return self.get();
    }


    Auth::Private::Private(Auth *parent)
            : QObject(parent)
            , request(new AuthRequest(parent))
            , child(new QProcess(this))
            , id(lastId++) {
        SocketServer::instance()->helpers[id] = this;
        QProcessEnvironment env = child->processEnvironment();
        bool langEmpty = true;
        QFile localeFile(QStringLiteral("/etc/locale.conf"));
        if (localeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&localeFile);
            while (!in.atEnd()) {
                QStringList parts = in.readLine().split(QLatin1Char('='));
                if (parts.size() >= 2) {
                    env.insert(parts[0], parts[1]);
                    if (parts[0] == QLatin1String("LANG"))
                        langEmpty = false;
                }
            }
            localeFile.close();
        }
        if (langEmpty)
            env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
        child->setProcessEnvironment(env);
        connect(child, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &Auth::Private::childExited);
        connect(child, &QProcess::errorOccurred, this, &Auth::Private::childError);
        connect(request, &AuthRequest::finished, this, &Auth::Private::requestFinished);
        connect(request, &AuthRequest::promptsChanged, parent, &Auth::requestChanged);
    }

    Auth::Private::~Private()
    {
        SocketServer::instance()->helpers.remove(id);
    }


    bool Auth::Private::acceptHello(QLocalSocket *socket) {
#ifdef SO_PEERCRED
        struct ucred credentials { };
        socklen_t size = sizeof(credentials);
        return getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0
            && credentials.pid == child->processId() && credentials.uid == geteuid();
#else
        Q_UNUSED(socket);
        return false;
#endif
    }

    void Auth::Private::setSocket(QLocalSocket *socket) {
        this->socket = socket;
        connect(socket, &QLocalSocket::readyRead, this, &Auth::Private::dataPending);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (this->socket == socket) {
                this->socket = nullptr;
                // A successfully authorized helper can close its socket during
                // pam_end while QProcess still reports Running. Let finished()
                // observe its real exit status instead of killing it here.
                if (!stopping && !authorizationSent && child->state() != QProcess::NotRunning) {
                    const quint64 generation = launchGeneration;
                    child->terminate();
                    QTimer::singleShot(5000, this, [this, generation] {
                        if (launchGeneration == generation && child->state() != QProcess::NotRunning)
                            child->kill();
                    });
                }
            }
            socket->deleteLater();
        });
    }

    void Auth::Private::dataPending() {
        Auth *auth = qobject_cast<Auth*>(parent());
        Msg m = MSG_UNKNOWN;
        while (socket && socket->bytesAvailable() >= qint64(sizeof(qint64))) {
            qint64 length = -1;
            if (socket->peek(reinterpret_cast<char *>(&length), sizeof(length)) != sizeof(length)
                || length < 0 || length > 1024 * 1024) {
                child->kill();
                return;
            }
            if (socket->bytesAvailable() < qint64(sizeof(length)) + length)
                return;
            socket->read(reinterpret_cast<char *>(&length), sizeof(length));
            QByteArray payload = socket->read(length);
            QDataStream str(payload);
            QString incomingRequestId;
            AuthMethod incomingMethod = AuthMethod::Password;
            str >> m >> incomingRequestId >> incomingMethod;
            if (str.status() != QDataStream::Ok || incomingRequestId != requestId || incomingMethod != method) {
                qWarning() << "Auth: rejected helper message with mismatched attempt identity";
                child->kill();
                return;
            }
            switch (m) {
                case ERROR: {
                    QString message;
                    Error type = ERROR_NONE;
                    str >> message >> type;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    Q_EMIT auth->error(message, type);
                    break;
                }
                case INFO: {
                    QString message;
                    Info type = INFO_NONE;
                    str >> message >> type;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    Q_EMIT auth->info(message, type);
                    break;
                }
                case REQUEST: {
                    Request r;
                    str >> r;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    request->setRequest(&r);
                    break;
                }
                case AUTHENTICATED: {
                    QString user;
                    str >> user;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    if (!user.isEmpty()) {
                        awaitingAuthorization = true;
                        auth->setUser(user);
                        Q_EMIT auth->authentication(user, true);
                        if (!deferredAuthorization)
                            auth->authorizeSession(true);
                    } else {
                        Q_EMIT auth->authentication(user, false);
                    }
                    break;
                }
                case SESSION_STATUS: {
                    bool status;
                    str >> status;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    Q_EMIT auth->sessionStarted(status);
                    SafeDataStream reply(socket);
                    reply << SESSION_STATUS << this->requestId << this->method;
                    reply.send();
                    break;
                }
                case DISPLAY_SERVER_STARTED: {
                    QString displayName;
                    str >> displayName;
                    if (str.status() != QDataStream::Ok || !str.atEnd()) { child->kill(); return; }
                    Q_EMIT auth->displayServerReady(displayName);
                    SafeDataStream reply(socket);
                    reply << DISPLAY_SERVER_STARTED << this->requestId << this->method;
                    reply.send();
                    break;
                }
                default: {
                    qWarning() << "Auth: rejected unknown helper opcode" << qint32(m);
                    child->kill();
                    return;
                }
            }
        }
    }

    void Auth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
        helloAccepted = false;
        awaitingAuthorization = false;
        authorizationSent = false;
        if (socket) {
            QLocalSocket *oldSocket = socket;
            socket = nullptr;
            oldSocket->disconnect(this);
            oldSocket->abort();
            oldSocket->deleteLater();
        }
        Auth::HelperExitStatus status = Auth::HELPER_OTHER_ERROR;
        if (exitStatus == QProcess::NormalExit
            && exitCode >= Auth::HELPER_SUCCESS && exitCode <= Auth::HELPER_TTY_ERROR) {
            status = Auth::HelperExitStatus(exitCode);
        } else {
            qWarning("Auth: sddm-helper exited abnormally (exit code %d)", exitCode);
            Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
        }

        if (status == Auth::HELPER_SUCCESS)
            qDebug() << "Auth: sddm-helper exited successfully";
        else
            qWarning() << "Auth: sddm-helper exited with status" << status;

        Q_EMIT qobject_cast<Auth*>(parent())->finished(status);
    }

    void Auth::Private::childError(QProcess::ProcessError error) {
        Q_UNUSED(error);
        Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
    }

    void Auth::Private::requestFinished() {
        SafeDataStream str(socket);
        Request r = request->request();
        str << REQUEST << this->requestId << this->method << r;
        str.send();
        request->setRequest();
    }


    Auth::Auth(const QString &user, const QString &session, bool autologin, QObject *parent, bool verbose)
            : QObject(parent)
            , d(new Private(this)) {
        setUser(user);
        setAutologin(autologin);
        setSession(session);
        setVerbose(verbose);
    }

    Auth::Auth(QObject* parent)
            : QObject(parent)
            , d(new Private(this)) {
    }

    Auth::~Auth() {
        stop();
        delete d;
    }

    void Auth::registerTypes() {
        qmlRegisterAnonymousType<AuthPrompt>("Auth", 1);
        qmlRegisterAnonymousType<AuthRequest>("Auth", 1);
        qmlRegisterType<Auth>("Auth", 1, 0, "Auth");
    }

    bool Auth::autologin() const {
        return d->autologin;
    }

    bool Auth::isGreeter() const
    {
        return d->greeter;
    }

    const QByteArray& Auth::cookie() const {
        return d->cookie;
    }

    const QString &Auth::session() const {
        return d->sessionPath;
    }

    const QString &Auth::user() const {
        return d->user;
    }

    bool Auth::verbose() const {
        return d->child->processChannelMode() == QProcess::ForwardedChannels;
    }

    AuthRequest *Auth::request() {
        return d->request;
    }

    quint64 Auth::launchGeneration() const {
        return d->launchGeneration;
    }

    AuthMethod Auth::method() const {
        return d->method;
    }

    const QString &Auth::requestId() const {
        return d->requestId;
    }

    bool Auth::isDeferredAuthorization() const {
        return d->deferredAuthorization;
    }

    bool Auth::isActive() const {
        return d->child->state() != QProcess::NotRunning;
    }
    void Auth::insertEnvironment(const QProcessEnvironment &env) {
        d->environment.insert(env);
    }

    void Auth::insertEnvironment(const QString &key, const QString &value) {
        d->environment.insert(key, value);
    }

    void Auth::setCookie(const QByteArray& cookie) {
        if (cookie != d->cookie) {
            d->cookie = cookie;
            Q_EMIT cookieChanged();
        }
    }

    void Auth::setUser(const QString &user) {
        if (user != d->user) {
            d->user = user;
            Q_EMIT userChanged();
        }
    }

    void Auth::setAutologin(bool on) {
        if (on != d->autologin) {
            d->autologin = on;
            Q_EMIT autologinChanged();
        }
    }

    void Auth::setGreeter(bool on)
    {
        if (on != d->greeter) {
            d->greeter = on;
            Q_EMIT greeterChanged();
        }
    }

    void Auth::setDisplayServerCommand(const QString &command)
    {
        if (d->displayServerCmd != command) {
            d->displayServerCmd = command;
            Q_EMIT displayServerCommandChanged();
        }
    }

    void Auth::setSession(const QString& path) {
        if (path != d->sessionPath) {
            d->sessionPath = path;
            Q_EMIT sessionChanged();
        }
    }

    void Auth::setMethod(AuthMethod method) {
        if (d->child->state() != QProcess::NotRunning
            || (method != AuthMethod::Password && method != AuthMethod::Fingerprint))
            return;
        if (d->method != method) {
            d->method = method;
            Q_EMIT methodChanged();
        }
    }

    void Auth::setRequestId(const QString &requestId) {
        if (d->child->state() != QProcess::NotRunning || QUuid(requestId).isNull())
            return;
        if (d->requestId != requestId) {
            d->requestId = requestId;
            Q_EMIT requestIdChanged();
        }
    }

    void Auth::setDeferredAuthorization(bool deferred) {
        if (d->child->state() == QProcess::NotRunning)
            d->deferredAuthorization = deferred;
    }

    void Auth::authorizeSession(bool proceed) {
        if (!d->awaitingAuthorization || d->authorizationSent || !d->socket
            || d->socket->state() != QLocalSocket::ConnectedState)
            return;
        d->authorizationSent = true;
        d->awaitingAuthorization = false;
        SafeDataStream str(d->socket);
        str << SESSION_AUTHORIZATION << d->requestId << d->method << proceed;
        if (proceed)
            str << d->environment << d->cookie;
        str.send();
    }

    void Auth::setVerbose(bool on) {
        if (on != verbose()) {
            if (on)
                d->child->setProcessChannelMode(QProcess::ForwardedChannels);
            else
                d->child->setProcessChannelMode(QProcess::SeparateChannels);
            Q_EMIT verboseChanged();
        }
    }

    void Auth::start() {
        if (d->child->state() != QProcess::NotRunning)
            return;
        const quint64 generation = ++d->launchGeneration;
        d->helloAccepted = false;
        d->awaitingAuthorization = false;
        d->authorizationSent = false;
        d->stopping = false;
        if (d->socket) {
            d->socket->disconnect(d);
            d->socket->abort();
            d->socket->deleteLater();
            d->socket = nullptr;
        }
        QStringList args;
        args << QStringLiteral("--socket") << SocketServer::instance()->fullServerName();
        args << QStringLiteral("--id") << QString::number(d->id);
        args << QStringLiteral("--request-id") << d->requestId;
        args << QStringLiteral("--method") << QString::number(qint32(d->method));
        if (d->deferredAuthorization)
            args << QStringLiteral("--defer-auth");
        if (!d->sessionPath.isEmpty())
            args << QStringLiteral("--start") << d->sessionPath;
        if (!d->user.isEmpty())
            args << QStringLiteral("--user") << d->user;
        if (d->autologin)
            args << QStringLiteral("--autologin");
        if (!d->displayServerCmd.isEmpty())
            args << QStringLiteral("--display-server") << d->displayServerCmd;
        if (d->greeter)
            args << QStringLiteral("--greeter");
        d->child->start(QStringLiteral("%1/sddm-helper").arg(QStringLiteral(LIBEXEC_INSTALL_DIR)), args);
        QTimer::singleShot(5000, d, [privateData = d, generation] {
            if (privateData->launchGeneration == generation
                && privateData->child->state() != QProcess::NotRunning && !privateData->helloAccepted)
                privateData->child->kill();
        });
    }

    void Auth::terminate() {
        if (d->child->state() != QProcess::NotRunning)
            d->child->terminate();
    }

    void Auth::kill() {
        if (d->child->state() != QProcess::NotRunning)
            d->child->kill();
    }

    void Auth::stop() {
        d->stopping = true;
        if (d->child->state() == QProcess::NotRunning)
            return;
        terminate();
        if (!d->child->waitForFinished(5000)) {
            kill();
            d->child->waitForFinished(1000);
        }
    }
}

#include "Auth.moc"
