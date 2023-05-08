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


    void Auth::Private::setSocket(QLocalSocket *socket) {
        this->socket = socket;
        connect(socket, &QLocalSocket::readyRead, this, &Auth::Private::dataPending);
    }

    void Auth::Private::dataPending() {
        Auth *auth = qobject_cast<Auth*>(parent());
        Msg m = MSG_UNKNOWN;
        SafeDataStream str(socket);
        while (socket->bytesAvailable() > 0) {
            str.receive();
            str >> m;
            switch (m) {
                case ERROR: {
                    QString message;
                    Error type = ERROR_NONE;
                    str >> message >> type;
                    Q_EMIT auth->error(message, type);
                    break;
                }
                case INFO: {
                    QString message;
                    Info type = INFO_NONE;
                    str >> message >> type;
                    Q_EMIT auth->info(message, type);
                    break;
                }
                case REQUEST: {
                    Request r;
                    str >> r;
                    request->setRequest(&r);
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
                    str >> status;
                    Q_EMIT auth->sessionStarted(status);
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
                    Q_EMIT auth->error(QStringLiteral("Auth: Unexpected value received: %1").arg(m), ERROR_INTERNAL);
                }
            }
        }
    }

    void Auth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit) {
            qWarning("Auth: sddm-helper (%s) crashed (exit code %d)",
                     qPrintable(child->arguments().join(QLatin1Char(' '))),
                     HelperExitStatus(exitStatus));
            Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
        }

        if (exitCode == HELPER_SUCCESS)
            qDebug() << "Auth: sddm-helper exited successfully";
        else
            qWarning("Auth: sddm-helper exited with %d", exitCode);

        Q_EMIT qobject_cast<Auth*>(parent())->finished((Auth::HelperExitStatus)exitCode);
    }

    void Auth::Private::childError(QProcess::ProcessError error) {
        Q_UNUSED(error);
        Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
    }

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
        args << QStringLiteral("--id") << QString::number(d->id);
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

    void Auth::stop() {
        if (d->child->state() == QProcess::NotRunning) {
            return;
        }

        d->child->terminate();

        // wait for finished
        if (!d->child->waitForFinished(5000))
            d->child->kill();
    }
}

#include "Auth.moc"
