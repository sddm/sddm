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
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#if QT_VERSION >= 0x050000
# include <QtQml/QtQml>
#else
# include <QtDeclarative/QtDeclarative>
#endif

#include <unistd.h>

class Auth::SocketServer : public QLocalServer {
    Q_OBJECT
public slots:
    void handleNewConnection();
public:
    static SocketServer *instance();

    QMap<qint64, Auth::Private*> helpers;
private:
    static Auth::SocketServer *self;
    SocketServer();
};

Auth::SocketServer *Auth::SocketServer::self = nullptr;

class Auth::Private : public QObject {
    Q_OBJECT
public:
    Private(Auth *parent);
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
    QString sessionPath { };
    QString user { };
    bool autologin { false };
    QProcessEnvironment environment { };
    qint64 id { 0 };
    static qint64 lastId;
};

qint64 Auth::Private::lastId = 1;



Auth::SocketServer::SocketServer()
        : QLocalServer() {
    connect(this, SIGNAL(newConnection()), this, SLOT(handleNewConnection()));
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
    if (!self) {
        self = new SocketServer();
        // TODO until i'm not too lazy to actually hash something
        self->listen(QString("Auth%1.%2").arg(getpid()).arg(time(NULL)));
    }
    return self;
}


Auth::Private::Private(Auth *parent)
        : QObject(parent)
        , request(new AuthRequest(parent))
        , child(new QProcess(this))
        , id(lastId++) {
    SocketServer::instance()->helpers[id] = this;
    QProcessEnvironment env = child->processEnvironment();
    env.insert("LANG", "C");
    child->setProcessEnvironment(env);
    connect(child, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(childExited(int,QProcess::ExitStatus)));
    connect(child, SIGNAL(error(QProcess::ProcessError)), this, SLOT(childError(QProcess::ProcessError)));
    connect(request, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(request, SIGNAL(promptsChanged()), parent, SIGNAL(requestChanged()));
}

void Auth::Private::setSocket(QLocalSocket *socket) {
    this->socket = socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(dataPending()));
}

void Auth::Private::dataPending() {
    Auth *auth = qobject_cast<Auth*>(parent());
    Msg m = MSG_UNKNOWN;
    SafeDataStream str(socket);
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
                str << AUTHENTICATED << environment;
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
            Q_EMIT auth->session(status);
            str.reset();
            str << SESSION_STATUS;
            str.send();
            break;
        }
        default: {
            Q_EMIT auth->error(QString("Auth: Unexpected value received: %1").arg(m), ERROR_INTERNAL);
        }
    }
}

void Auth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus != QProcess::NormalExit) {
        qWarning("Auth: sddm-helper crashed (exit code %d)", exitCode);
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

void Auth::insertEnvironment(const QProcessEnvironment &env) {
    d->environment.insert(env);
}

void Auth::insertEnvironment(const QString &key, const QString &value) {
    d->environment.insert(key, value);
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
    args << "--socket" << SocketServer::instance()->fullServerName();
    args << "--id" << QString("%1").arg(d->id);
    if (!d->sessionPath.isEmpty())
        args << "--start" << d->sessionPath;
    if (!d->user.isEmpty())
        args << "--user" << d->user;
    if (d->autologin)
        args << "--autologin";
    d->child->start(QString("%1/sddm-helper").arg(LIBEXEC_INSTALL_DIR), args);
}

#include "Auth.moc"
