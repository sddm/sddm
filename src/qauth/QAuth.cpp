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

#include "QAuth.h"
#include "Constants.h"
#include "QAuthMessages.h"
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

class QAuth::SocketServer : public QLocalServer {
    Q_OBJECT
public slots:
    void handleNewConnection();
public:
    static SocketServer *instance();

    QMap<qint64, QAuth::Private*> helpers;
private:
    static QAuth::SocketServer *self;
    SocketServer();
};

QAuth::SocketServer *QAuth::SocketServer::self = nullptr;

class QAuth::Private : public QObject {
    Q_OBJECT
public:
    Private(QAuth *parent);
    void setSocket(QLocalSocket *socket);
public slots:
    void dataPending();
    void childExited(int exitCode, QProcess::ExitStatus exitStatus);
    void childError(QProcess::ProcessError error);
    void requestFinished();
public:
    QAuthRequest *request { nullptr };
    QProcess *child { nullptr };
    QLocalSocket *socket { nullptr };
    QString sessionPath { };
    QString user { };
    bool autologin { false };
    QProcessEnvironment environment { };
    qint64 id { 0 };
    static qint64 lastId;
};

qint64 QAuth::Private::lastId = 1;



QAuth::SocketServer::SocketServer()
        : QLocalServer() {
    connect(this, SIGNAL(newConnection()), this, SLOT(handleNewConnection()));
}

void QAuth::SocketServer::handleNewConnection()  {
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

QAuth::SocketServer* QAuth::SocketServer::instance() {
    if (!self) {
        self = new SocketServer();
        // TODO until i'm not too lazy to actually hash something
        self->listen(QString("QAuth%1.%2").arg(getpid()).arg(time(NULL)));
    }
    return self;
}


QAuth::Private::Private(QAuth *parent)
        : QObject(parent)
        , request(new QAuthRequest(parent))
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

void QAuth::Private::setSocket(QLocalSocket *socket) {
    this->socket = socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(dataPending()));
}

void QAuth::Private::dataPending() {
    QAuth *auth = qobject_cast<QAuth*>(parent());
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
            Q_EMIT auth->error(QString("QAuth: Unexpected value received: %1").arg(m), ERROR_INTERNAL);
        }
    }
}

void QAuth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus != QProcess::NormalExit)
        Q_EMIT qobject_cast<QAuth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
    Q_EMIT qobject_cast<QAuth*>(parent())->finished(!exitCode);
}

void QAuth::Private::childError(QProcess::ProcessError error) {
    Q_UNUSED(error);
    Q_EMIT qobject_cast<QAuth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
}

void QAuth::Private::requestFinished() {
    SafeDataStream str(socket);
    Request r = request->request();
    str << REQUEST << r;
    str.send();
    request->setRequest();
}


QAuth::QAuth(const QString &user, const QString &session, bool autologin, QObject *parent, bool verbose)
        : QObject(parent)
        , d(new Private(this)) {
    setUser(user);
    setAutologin(autologin);
    setSession(session);
    setVerbose(verbose);
}

QAuth::QAuth(QObject* parent)
        : QObject(parent)
        , d(new Private(this)) {
}

QAuth::~QAuth() {
    delete d;
}

void QAuth::registerTypes() {
    qmlRegisterType<QAuthPrompt>();
    qmlRegisterType<QAuthRequest>();
    qmlRegisterType<QAuth>("QAuth", 1, 0, "QAuth");
}

bool QAuth::autologin() const {
    return d->autologin;
}

const QString &QAuth::session() const {
    return d->sessionPath;
}

const QString &QAuth::user() const {
    return d->user;
}

bool QAuth::verbose() const {
    return d->child->processChannelMode() == QProcess::ForwardedChannels;
}

QAuthRequest *QAuth::request() {
    return d->request;
}

void QAuth::insertEnvironment(const QProcessEnvironment &env) {
    d->environment.insert(env);
}

void QAuth::insertEnvironment(const QString &key, const QString &value) {
    d->environment.insert(key, value);
}

void QAuth::setUser(const QString &user) {
    if (user != d->user) {
        d->user = user;
        Q_EMIT userChanged();
    }
}

void QAuth::setAutologin(bool on) {
    if (on != d->autologin) {
        d->autologin = on;
        Q_EMIT autologinChanged();
    }
}

void QAuth::setSession(const QString& path) {
    if (path != d->sessionPath) {
        d->sessionPath = path;
        Q_EMIT sessionChanged();
    }
}

void QAuth::setVerbose(bool on) {
    if (on != verbose()) {
        if (on)
            d->child->setProcessChannelMode(QProcess::ForwardedChannels);
        else
            d->child->setProcessChannelMode(QProcess::SeparateChannels);
        Q_EMIT verboseChanged();
    }
}

void QAuth::start() {
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

#include "QAuth.moc"
