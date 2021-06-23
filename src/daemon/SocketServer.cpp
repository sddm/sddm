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

#include "SocketServer.h"

#include "DaemonApp.h"
#include "Messages.h"
#include "PowerManager.h"
#include "SocketWriter.h"
#include "Utils.h"

#include "AuthBase.h"

#include <QLocalServer>

namespace SDDM {
    SocketServer::SocketServer(QObject *parent) : QObject(parent) {
    }

    QString SocketServer::socketAddress() const {
        if (m_server)
            return m_server->fullServerName();
        return QString();
    }

    bool SocketServer::start(const QString &displayName) {
        // check if the server has been created already
        if (m_server)
            return false;

        QString socketName = QStringLiteral("sddm-%1-%2").arg(displayName).arg(Utils::generateName(6));

        // log message
        qDebug() << "Socket server starting...";

        // create server
        m_server = new QLocalServer(this);

        // set server options
        m_server->setSocketOptions(QLocalServer::UserAccessOption);

        // start listening
        if (!m_server->listen(socketName)) {
            // log message
            qCritical() << "Failed to start socket server.";

            // return fail
            return false;
        }


        // log message
        qDebug() << "Socket server started.";

        // connect signals
        connect(m_server, &QLocalServer::newConnection, this, &SocketServer::newConnection);

        // return success
        return true;
    }

    void SocketServer::stop() {
        // check flag
        if (!m_server)
            return;

        // log message
        qDebug() << "Socket server stopping...";

        // delete server
        m_server->deleteLater();
        m_server = nullptr;

        // log message
        qDebug() << "Socket server stopped.";
    }

    void SocketServer::newConnection() {
        // get pending connection
        QLocalSocket *socket = m_server->nextPendingConnection();

        // connect signals
        connect(socket, &QLocalSocket::readyRead, this, &SocketServer::readyRead);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }

    // from greeter (to backend)

    void SocketServer::readyRead() {
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());

        static const char *logPrefix = "SocketServer: Message received from greeter:";

        // check socket
        if (!socket)
            return;

        // input stream
        QDataStream input(socket);

        // read message
        quint32 message;
        input >> message;

        switch (GreeterMessages(message)) {
            case GreeterMessages::Connect: {
                // log message
                qDebug() << logPrefix << "Connect";

                // send capabilities
                SocketWriter(socket) << quint32(DaemonMessages::Capabilities) << quint32(daemonApp->powerManager()->capabilities());

                // send host name
                SocketWriter(socket) << quint32(DaemonMessages::HostName) << daemonApp->hostName();

                // emit signal
                emit connected();
            }
            break;
            case GreeterMessages::Login: {
                // log message
                qDebug() << logPrefix << "Login";

                // read username, pasword etc.
                QString user, password, filename;
                Session session;
                input >> user >> password >> session;

                // emit signal
                emit login(socket, user, password, session);
            }
            break;
            case GreeterMessages::PamResponse: {
                qDebug() << logPrefix << "PamResponse";

                // new password from greeter dialog
                QString newPassword;
                input >> newPassword;

                // emit signal
                emit sendPamResponse(newPassword);
            }
            break;
            case GreeterMessages::PamCancel: {
                qDebug() << logPrefix << "PamCancel";

                // emit signal
                emit cancelPamConv();
            }
            break;
            case GreeterMessages::PowerOff: {
                // log message
                qDebug() << logPrefix << "PowerOff";

                // power off
                daemonApp->powerManager()->powerOff();
            }
            break;
            case GreeterMessages::Reboot: {
                qDebug() << logPrefix << "Reboot";

                // reboot
                daemonApp->powerManager()->reboot();
            }
            break;
            case GreeterMessages::Suspend: {
                qDebug() << logPrefix << "Suspend";

                // suspend
                daemonApp->powerManager()->suspend();
            }
            break;
            case GreeterMessages::Hibernate: {
                qDebug() << logPrefix << "Hibernate";

                // hibernate
                daemonApp->powerManager()->hibernate();
            }
            break;
            case GreeterMessages::HybridSleep: {
                qDebug() << logPrefix << "HybridSleep";

                // hybrid sleep
                daemonApp->powerManager()->hybridSleep();
            }
            break;
            default: {
                qWarning() << logPrefix << "Unknown message" << message;
            }
        }
    }

    // from (pam) backend to greeter

    void SocketServer::loginFailed(QLocalSocket *socket, const QString &message, int result) {
        if(socket && socket->isValid())
            SocketWriter(socket) << quint32(DaemonMessages::LoginFailed) << message << result;
    }

    void SocketServer::loginSucceeded(QLocalSocket *socket) {
        if(socket && socket->isValid())
            SocketWriter(socket) << quint32(DaemonMessages::LoginSucceeded);
    }

    void SocketServer::pamConvMsg(QLocalSocket *socket, const QString &message, int result) {
        // send pam converse message to greeter
        if(socket && socket->isValid())
            SocketWriter(socket) << quint32(DaemonMessages::PamConvMsg) << message << result;
    }

    void SocketServer::pamRequest(QLocalSocket *socket, const AuthRequest * const pam_request) {
        if(socket && socket->isValid()) {
            // convert AuthRequest to simple Request
            Request request = pam_request->request();
            // send pam request to greeter
            SocketWriter(socket) << quint32(DaemonMessages::PamRequest) << request;
        }
    }
}
