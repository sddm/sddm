/***************************************************************************
* Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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
#include "AuthBase.h"
#include "AuthRequest.h"

#include <QLocalSocket>

namespace SDDM {
    class GreeterProxyPrivate {
    public:
        SessionModel *sessionModel { nullptr };
        QLocalSocket *socket { nullptr };
        AuthRequest *request { nullptr };
        QString hostName;
        bool canPowerOff { false };
        bool canReboot { false };
        bool canSuspend { false };
        bool canHibernate { false };
        bool canHybridSleep { false };
        bool enablePwdChange { false };
    };

    GreeterProxy::GreeterProxy(const QString &socket, QObject *parent) : QObject(parent), d(new GreeterProxyPrivate()) {
        d->socket = new QLocalSocket(this);
        // connect signals
        connect(d->socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
        connect(d->socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
        connect(d->socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
        connect(d->socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error), this, &GreeterProxy::error);
        // AuthRequest for exchange of PAM conversation prompts/responses with qml
        d->request = new AuthRequest(this);
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

    void GreeterProxy::enablePwdChange() {
        qDebug() << "Enable password change for theme";
        d->enablePwdChange = true;
    }

    /** AuthRequest for exchange of PAM prompts/responses
     *  between GreeterProxy and greeter GUI
     */
    AuthRequest *GreeterProxy::getRequest() {
        return d->request;
    }

    void GreeterProxy::setRequest(Request *r) {
        d->request->setRequest(r);
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

    // todo: mind session index like for login
    void GreeterProxy::pamResponse(const QString &newPassword) {
        // send new password to daemon for pam conv
        SocketWriter(d->socket) << quint32(GreeterMessages::PamResponse) << newPassword;
    }

    // todo: mind session index like for login
    void GreeterProxy::cancelPamConv() {
        // send command to daemon (to pam conv() in backend)
        SocketWriter(d->socket) << quint32(GreeterMessages::PamCancel);
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
                    QString m;
                    int rc;
                    // read pam conv() message (info/error) and pam rc from daemon
                    input >> m >> rc;
                    // log message
                    qDebug() << "Message received from daemon: LoginFailed, " << m << ", rc =" << rc;
                    // emit signal
                    emit loginFailed(m, rc);
                }
                break;
                // pam messages from conv() following login, e.g. for expired pwd
                // conv() msg with msg_style: PAM_ERROR_MSG or PAM_TEXT_INFO
                case DaemonMessages::PamConvMsg: {
                    QString m;
                    int rc;
                    // read pam conv() message (info/error) and pam rc from daemon
                    input >> m >> rc;
                    qDebug() << "PAM conversation message from daemon: " << m << ", rc =" << rc;
                    // send message to GUI
                    emit pamConvMsg(m, rc);
                }
                break;

                // new request with prompts from conv() after login, e.g. due to expired pwd
                // request contains prompt (message) from conv() with msg_style:
                // PAM_PROMPT_ECHO_ON (user name) or PAM_PROMPT_ECHO_OFF (passwords)
                case DaemonMessages::PamRequest: {
                    Request r;
                    // read request from daemon (Display->SocketServer)
                    input >> r;
                    // set pam request  for qml and convert Request to AuthRequest
                    setRequest(&r);
                    // log prompts
                    qDebug() << "PAM request with " << r.prompts.length() << " prompts from daemon";
                    for (const Prompt &p : r.prompts)
                        qDebug() << "GreeterProxy: Prompt message =" << p.message << ", hidden =" << p.hidden
                                        << ", type =" << AuthPrompt::typeToString(p.type) << "(" << p.type << ")";

                    // compatibility with old themes: cancel pam
                    // conversation if theme does not support it
                    if(!d->enablePwdChange) {
                        qDebug() << "Cancled password change. Not supported by theme.";
                        cancelPamConv();
                    } else
                        // otherwise send pam messages to GUI
                        emit pamRequest();
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
