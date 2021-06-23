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
#include "Configuration.h"
#include "AuthMessages.h"
#include "SafeDataStream.h"
#include "Utils.h"

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <QtQml/QtQml>

#include <memory>

#include <unistd.h>

namespace SDDM {
    class Auth::SocketServer : public QLocalServer {
        Q_OBJECT
    public slots:
        void handleNewConnection();
    public:
        static SocketServer *instance();

        QMap<qint64, Auth::Private*> helpers;
    private:
        SocketServer();
    };

    class Auth::Private : public QObject {
        Q_OBJECT
    public:
        Private(Auth *parent);
        ~Private();
        void setSocket(QLocalSocket *socket);
    public slots:
        void dataPending();
        void childExited(int exitCode, QProcess::ExitStatus exitStatus);
        void childError(QProcess::ProcessError error);
        void cancelPamConv();
        void requestFinished();
    public:
        AuthRequest *request { nullptr };
        QProcess *child { nullptr };
        QLocalSocket *socket { nullptr };
        QString displayServerCmd;
        QString sessionPath { };
        QString user { };
        QString cookie { };
        bool autologin { false };
        bool greeter { false };
        QProcessEnvironment environment { };
        qint64 sessionPid { -1 };
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
            Msg m = Msg::MSG_UNKNOWN;
            qint64 id;
            QLocalSocket *socket = nextPendingConnection();
            SafeDataStream str(socket);
            str.receive();
            str >> m >> id;
            if (m == Msg::HELLO && id && SocketServer::instance()->helpers.contains(id)) {
                helpers[id]->setSocket(socket);
                if (socket->bytesAvailable() > 0)
                    helpers[id]->dataPending();
            }
        }
    }

    Auth::SocketServer* Auth::SocketServer::instance() {
        static std::unique_ptr<Auth::SocketServer> self;
        if (!self) {
            self.reset(new SocketServer());
            self->listen(QStringLiteral("sddm-auth%1").arg(QUuid::createUuid().toString().replace(QRegExp(QStringLiteral("[{}]")), QString())));
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
        // read locale settings from distro specific file, default /etc/locale.conf
        Utils::readLocaleFile(env, mainConfig.LocaleFile.get());
        child->setProcessEnvironment(env);
        connect(child, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &Auth::Private::childExited);
        connect(child, QOverload<QProcess::ProcessError>::of(&QProcess::error), this, &Auth::Private::childError);
        connect(request, &AuthRequest::canceled, this, &Auth::Private::cancelPamConv);
        connect(request, &AuthRequest::finished, this, &Auth::Private::requestFinished);
        connect(request, &AuthRequest::promptsChanged, parent, &Auth::requestChanged);
    }

    Auth::Private::~Private()
    {
        SocketServer::instance()->helpers.remove(id);
    }


    void Auth::Private::setSocket(QLocalSocket *socket) {
        this->socket = socket;
        connect(socket, &QLocalSocket::readyRead, this, &Auth::Private::dataPending);
    }

    // from (PAM) Backend to daemon
    void Auth::Private::dataPending() {
        Auth *auth = qobject_cast<Auth*>(parent());
        Msg m = MSG_UNKNOWN;
        SafeDataStream str(socket);
        do {
            str.receive();
            str >> m;
            switch (m) {
                case ERROR: {
                    int result;
                    QString message;
                    AuthEnums::Error type = AuthEnums::ERROR_NONE;
                    str >> message >> type >> result;
                    Q_EMIT auth->error(message, type, result);
                    break;
                }
                case INFO: {
                    int result;
                    QString message;
                    AuthEnums::Info type = AuthEnums::INFO_NONE;
                    str >> message >> type >> result;
                    Q_EMIT auth->info(message, type, result);
                    break;
                }
                // request from (PAM) Backend
                case REQUEST: {
                    Request r;
                    str >> r;
                    request->setRequest(&r);
                    qDebug() << "Auth: Request received";
                    break;
                }
                case AUTHENTICATED: {
                    QString user;
                    str >> user;
                    if (!user.isEmpty()) {
                        auth->setUser(user);
                        Q_EMIT auth->authentication(user, true);
                        str.reset();
                        str << AUTHENTICATED << environment << cookie;
                        str.send();
                    }
                    else {
                        Q_EMIT auth->authentication(user, false);
                    }
                    break;
                }
                case SESSION_STATUS: {
                    bool status;
                    qint64 pid; //not pid_t as we need to define the wire type
                    str >> status >> pid;
                    sessionPid = pid;
                    Q_EMIT auth->sessionStarted(status, pid);
                    str.reset();
                    str << SESSION_STATUS;
                    str.send();
                    break;
                }
                case DISPLAY_SERVER_STARTED: {
                    QString displayName;
                    str >> displayName;
                    Q_EMIT auth->displayServerReady(displayName);
                    str.reset();
                    str << DISPLAY_SERVER_STARTED;
                    str.send();
                    break;
                }
                default: {
                    qWarning("Auth: dataPending:  Unknown message received: %d", m);
                    Q_EMIT auth->error(QStringLiteral("Auth: Unexpected value received: %1").arg(m), AuthEnums::ERROR_INTERNAL, 0);
                }
            }
        } while(str.available());
    }

    void Auth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit) {
            qWarning("Auth: sddm-helper (%s) crashed (exit status %d, exit code %d)",
                     qPrintable(child->arguments().join(QLatin1Char(' '))),
                     AuthEnums::HelperExitStatus(exitStatus), exitCode);
            Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), AuthEnums::ERROR_INTERNAL, 0);
        }

        if (exitCode == AuthEnums::HELPER_SUCCESS)
            qDebug() << "Auth: sddm-helper exited successfully";
        else
            qWarning("Auth: sddm-helper exited with %d", exitCode);

        Q_EMIT qobject_cast<Auth*>(parent())->finished((AuthEnums::HelperExitStatus)exitCode);
    }

    void Auth::Private::childError(QProcess::ProcessError error) {
        Q_UNUSED(error);
        Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), AuthEnums::ERROR_INTERNAL, 0);
    }

    // from daemon to (PAM) backend
    void Auth::Private::cancelPamConv() {
        SafeDataStream str(socket);
        qDebug() << "Auth: cancelPamConv, send CANCEL to backend";
        str << CANCEL;
        str.send();
        request->setRequest();
    }

    // from daemon to (PAM) backend
    void Auth::Private::requestFinished() {
        SafeDataStream str(socket);
        Request r = request->request();
        str << REQUEST << r;
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
        delete d;
    }

    void Auth::registerTypes() {
        qmlRegisterType<AuthPrompt>();
        qmlRegisterType<AuthRequest>();
        qmlRegisterType<Auth>("Auth", 1, 0, "Auth");
    }

    bool Auth::autologin() const {
        return d->autologin;
    }

    bool Auth::isGreeter() const
    {
        return d->greeter;
    }

    const QString& Auth::cookie() const {
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

    qint64 Auth::sessionPid() const {
        return d->sessionPid;
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

    void Auth::setCookie(const QString& cookie) {
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
        QStringList args;
        args << QStringLiteral("--socket") << SocketServer::instance()->fullServerName();
        args << QStringLiteral("--id") << QStringLiteral("%1").arg(d->id);
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
    }
}

#include "Auth.moc"
