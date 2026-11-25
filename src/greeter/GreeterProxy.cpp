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
        bool canPowerOff { false };
        bool canReboot { false };
        bool canSuspend { false };
        bool canHibernate { false };
        bool canHybridSleep { false };
        bool authenticationMethodsAvailable { false };
        QString fingerprintState;
        QString fingerprintRequestId;
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

    bool GreeterProxy::authenticationMethodsAvailable() const {
        return d->authenticationMethodsAvailable;
    }

    const QString &GreeterProxy::fingerprintState() const {
        return d->fingerprintState;
    }

    const QString &GreeterProxy::fingerprintRequestId() const {
        return d->fingerprintRequestId;
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

    void GreeterProxy::startFingerprint(const QString &user, const int sessionIndex) const {
        if (!d->sessionModel || !d->authenticationMethodsAvailable)
            return;
        const QModelIndex index = d->sessionModel->index(sessionIndex, 0);
        const auto type = static_cast<Session::Type>(d->sessionModel->data(index, SessionModel::TypeRole).toInt());
        const QString name = d->sessionModel->data(index, SessionModel::FileRole).toString();
        SocketWriter(d->socket) << quint32(GreeterMessages::StartFingerprint) << user << Session(type, name);
    }

    void GreeterProxy::cancelFingerprint() const {
        if (d->fingerprintRequestId.isEmpty())
            return;
        SocketWriter(d->socket) << quint32(GreeterMessages::CancelFingerprint) << d->fingerprintRequestId;
    }

    void GreeterProxy::connected() {
        // log connection
        qDebug() << "Connected to the daemon.";

        // send connected message
        SocketWriter(d->socket) << quint32(GreeterMessages::Connect);
    }

    void GreeterProxy::disconnected() {
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
                    input >> d->hostName;

                    // emit signal
                    emit hostNameChanged(d->hostName);
                }
                break;
                case DaemonMessages::LoginSucceeded: {
                    // log message
                    qDebug() << "Message received from daemon: LoginSucceeded";

                    // emit signal
                    emit loginSucceeded();
                }
                break;
                case DaemonMessages::LoginFailed: {
                    // log message
                    qDebug() << "Message received from daemon: LoginFailed";

                    // emit signal
                    emit loginFailed();
                }
                break;
                case DaemonMessages::AuthenticationCapabilities: {
                    quint32 capabilities;
                    input >> capabilities;
                    const bool available = capabilities & 0x1;
                    if (d->authenticationMethodsAvailable != available) {
                        d->authenticationMethodsAvailable = available;
                        emit authenticationMethodsAvailableChanged(available);
                    }
                }
                break;
                case DaemonMessages::AuthenticationMethodState: {
                    QString requestId, detail;
                    quint32 state;
                    input >> requestId >> state >> detail;
                    if (d->fingerprintRequestId != requestId) {
                        d->fingerprintRequestId = requestId;
                        emit fingerprintRequestIdChanged(requestId);
                    }
                    Q_UNUSED(state);
                    if (d->fingerprintState != detail) {
                        d->fingerprintState = detail;
                        emit fingerprintStateChanged(detail);
                    }
                }
                break;
                case DaemonMessages::AuthenticationAttemptFinished: {
                    QString requestId;
                    quint32 success;
                    quint32 method;
                    input >> requestId >> success >> method;
                    Q_UNUSED(success);
                    Q_UNUSED(method);
                    if (d->fingerprintRequestId == requestId) {
                        d->fingerprintRequestId.clear();
                        emit fingerprintRequestIdChanged(d->fingerprintRequestId);
                    }
                }
                break;
                case DaemonMessages::InformationMessage: {
                    QString message;
                    input >> message;

                    qDebug() << "Information Message received from daemon: " << message;
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
