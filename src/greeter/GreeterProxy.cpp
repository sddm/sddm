/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include "GreeterProxy.h"

#include "Configuration.h"
#include "Messages.h"
#include "SessionModel.h"
#include "SocketWriter.h"

#include <QLocalSocket>

namespace SDDM {
    class GreeterProxyPrivate {
    public:
        SessionModel *sessionModel { nullptr };
        QLocalSocket *socket { nullptr };
        QString hostName;
        quint32 authenticationConversationVersion {0};
        quint32 nextConversation {0};
        bool canPowerOff { false };
        bool canReboot { false };
        bool canSuspend { false };
        bool canHibernate { false };
        bool canHybridSleep { false };
    };

    GreeterProxy::GreeterProxy(const QString &socket, QObject *parent) : QObject(parent), d(new GreeterProxyPrivate()) {
        d->socket = new QLocalSocket(this);
        // connect signals
        connect(d->socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
        connect(d->socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
        connect(d->socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
        connect(d->socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);

        // connect to server
        d->socket->connectToServer(socket);
    }

    GreeterProxy::~GreeterProxy() {
        delete d;
    }

    const QString &GreeterProxy::hostName() const {
        return d->hostName;
    }

    void GreeterProxy::setSessionModel(SessionModel *model) {
        d->sessionModel = model;
    }

    bool GreeterProxy::canPowerOff() const {
        return d->canPowerOff;
    }

    bool GreeterProxy::canReboot() const {
        return d->canReboot;
    }

    bool GreeterProxy::canSuspend() const {
        return d->canSuspend;
    }

    bool GreeterProxy::canHibernate() const {
        return d->canHibernate;
    }

    bool GreeterProxy::canHybridSleep() const {
        return d->canHybridSleep;
    }

    bool GreeterProxy::isConnected() const {
        return d->socket->state() == QLocalSocket::ConnectedState;
    }

    void GreeterProxy::powerOff() {
        SocketWriter(d->socket) << quint32(GreeterMessages::PowerOff);
    }

    void GreeterProxy::reboot() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Reboot);
    }

    void GreeterProxy::suspend() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Suspend);
    }

    void GreeterProxy::hibernate() {
        SocketWriter(d->socket) << quint32(GreeterMessages::Hibernate);
    }

    void GreeterProxy::hybridSleep() {
        SocketWriter(d->socket) << quint32(GreeterMessages::HybridSleep);
    }

    void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex) const {
        if (!d->sessionModel) {
            // log error
            qCritical() << "Session model is not set.";

            // return
            return;
        }

        // get model index
        QModelIndex index = d->sessionModel->index(sessionIndex, 0);

        // send command to the daemon
        Session::Type type = static_cast<Session::Type>(d->sessionModel->data(index, SessionModel::TypeRole).toInt());
        QString name = d->sessionModel->data(index, SessionModel::FileRole).toString();
        Session session(type, name);
        SocketWriter(d->socket) << quint32(GreeterMessages::Login) << user << password << session;
    }

    quint32 GreeterProxy::authenticationConversationVersion() const { return d->authenticationConversationVersion; }
    quint32 GreeterProxy::beginAuthentication(const QString &user, int sessionIndex) {
        if (d->authenticationConversationVersion != 1 || !d->sessionModel || user.isEmpty() || user.size() > 256
            || user.startsWith(QLatin1Char('-')) || user.contains(QChar(0)) || d->nextConversation == 0xffffffffu) return 0;
        const QModelIndex index = d->sessionModel->index(sessionIndex, 0);
        if (!index.isValid()) return 0;
        const auto type = static_cast<Session::Type>(d->sessionModel->data(index, SessionModel::TypeRole).toInt());
        const auto name = d->sessionModel->data(index, SessionModel::FileRole).toString();
        const quint32 attempt = ++d->nextConversation;
        SocketWriter(d->socket) << quint32(GreeterMessages::BeginConversation) << attempt << user << Session(type, name);
        return attempt;
    }
    void GreeterProxy::respond(quint32 attempt, quint32 prompt, const QString &response) {
        if (d->authenticationConversationVersion == 1 && attempt && prompt && response.size() <= 4096)
            SocketWriter(d->socket) << quint32(GreeterMessages::ConversationResponse) << attempt << prompt << response;
    }
    void GreeterProxy::cancelAuthentication(quint32 attempt) {
        if (d->authenticationConversationVersion == 1 && attempt)
            SocketWriter(d->socket) << quint32(GreeterMessages::CancelConversation) << attempt;
    }

    void GreeterProxy::connected() {
        // log connection
        qDebug() << "Connected to the daemon.";

        // send connected message
        SocketWriter(d->socket) << quint32(GreeterMessages::Connect);
        SocketWriter(d->socket) << quint32(GreeterMessages::ConversationHello);
    }

    void GreeterProxy::disconnected() {
        d->authenticationConversationVersion = 0;
        emit authenticationConversationVersionChanged();
        // log disconnection
        qDebug() << "Disconnected from the daemon.";

        Q_EMIT socketDisconnected();
    }

    void GreeterProxy::error() {
        qCritical() << "Socket error: " << d->socket->errorString();
    }

    void GreeterProxy::readyRead() {
        // input stream
        QDataStream input(d->socket);

        while (input.device()->bytesAvailable()) {
            if (d->socket->bytesAvailable() < 4) return;
            input.startTransaction();
            // read message
            quint32 message;
            input >> message;

            switch (DaemonMessages(message)) {
                case DaemonMessages::Capabilities: {
                    // log message
                    qDebug() << "Message received from daemon: Capabilities";

                    // read capabilities
                    quint32 capabilities;
                    input >> capabilities;
                    if (!input.commitTransaction()) return;

                    // parse capabilities
                    d->canPowerOff = capabilities & Capability::PowerOff;
                    d->canReboot = capabilities & Capability::Reboot;
                    d->canSuspend = capabilities & Capability::Suspend;
                    d->canHibernate = capabilities & Capability::Hibernate;
                    d->canHybridSleep = capabilities & Capability::HybridSleep;

                    // emit signals
                    emit canPowerOffChanged(d->canPowerOff);
                    emit canRebootChanged(d->canReboot);
                    emit canSuspendChanged(d->canSuspend);
                    emit canHibernateChanged(d->canHibernate);
                    emit canHybridSleepChanged(d->canHybridSleep);
                }
                break;
                case DaemonMessages::HostName: {
                    // log message
                    qDebug() << "Message received from daemon: HostName";

                    // read host name
                    QString hostName;
                    input >> hostName;
                    if (!input.commitTransaction()) return;
                    d->hostName = hostName;

                    // emit signal
                    emit hostNameChanged(d->hostName);
                }
                break;
                case DaemonMessages::ConversationVersion: {
                    quint32 version = 0; input >> version;
                    if (!input.commitTransaction()) return;
                    d->authenticationConversationVersion = version; emit authenticationConversationVersionChanged(); break;
                }
                case DaemonMessages::ConversationAuthorized: {
                    quint32 attempt = 0; input >> attempt;
                    if (!input.commitTransaction()) return;
                    emit authenticationAuthorized(attempt);
                    break;
                }
                case DaemonMessages::ConversationPrompt: {
                    quint32 attempt = 0, prompt = 0, hidden = 0; QString text;
                    input >> attempt >> prompt >> text >> hidden;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok && hidden <= 1) emit authenticationPrompt(attempt, prompt, text, hidden == 0);
                    break;
                }
                case DaemonMessages::ConversationMessage: {
                    quint32 attempt = 0, error = 0; QString text;
                    input >> attempt >> text >> error;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok && error <= 1) emit authenticationMessage(attempt, text, error == 1);
                    break;
                }
                case DaemonMessages::ConversationFinished: {
                    quint32 attempt = 0, result = 0; input >> attempt >> result;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok) emit authenticationFinished(attempt, result);
                    break;
                }
                case DaemonMessages::LoginSucceeded: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from daemon: LoginSucceeded";

                    // emit signal
                    emit loginSucceeded();
                }
                break;
                case DaemonMessages::LoginFailed: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from daemon: LoginFailed";

                    // emit signal
                    emit loginFailed();
                }
                break;
                case DaemonMessages::InformationMessage: {
                    QString message;
                    input >> message;
                    if (!input.commitTransaction()) return;
                    emit informationMessage(message);
                }
                break;
                default: {
                    // log message
                    qWarning() << "Unknown message received from daemon.";
                }
            }
        }
    }
}
