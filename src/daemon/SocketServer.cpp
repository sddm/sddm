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

#include "SocketServer.h"

#include "DaemonApp.h"
#include "Messages.h"
#include "PowerManager.h"
#include "SocketWriter.h"
#include "Utils.h"

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

        QString socketName = QStringLiteral("sddm-%1-%2").arg(displayName).arg(generateName(6));

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
        socket->setReadBufferSize(1024 * 1024);

        // connect signals
        connect(socket, &QLocalSocket::readyRead, this, &SocketServer::readyRead);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }

    void SocketServer::readyRead() {
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());

        // check socket
        if (!socket)
            return;

        // input stream
        QDataStream input(socket);

        // Qt's QLocalSocket::readyRead is not designed to be called at every socket.write(),
        // so we need to use a loop to read all the signals.
        if (socket->bytesAvailable() >= 1024 * 1024) { socket->abort(); return; }
        while(socket->bytesAvailable() >= qint64(sizeof(quint32))) {
            input.startTransaction();
            quint32 message;
            input >> message;
            if (input.status() == QDataStream::ReadPastEnd) {
                input.rollbackTransaction();
                return;
            }
            if (input.status() != QDataStream::Ok) { socket->abort(); return; }

            switch (GreeterMessages(message)) {
                case GreeterMessages::Connect: {
                    // log message
                    qDebug() << "Message received from greeter: Connect";

                    // send capabilities
                    SocketWriter(socket) << quint32(DaemonMessages::Capabilities) << quint32(daemonApp->powerManager()->capabilities());

                    // send host name
                    SocketWriter(socket) << quint32(DaemonMessages::HostName) << daemonApp->hostName();
                    SocketWriter(socket) << quint32(DaemonMessages::AuthenticationCapabilities) << quint32(0x1);

                    // emit signal
                    emit connected();
                }
                break;
                case GreeterMessages::Login: {
                    qDebug() << "Message received from greeter: Login";
                    QString user, password;
                    Session session;
                    input >> user >> password >> session;
                    if (input.status() == QDataStream::ReadPastEnd) { input.rollbackTransaction(); return; }
                    if (input.status() != QDataStream::Ok) { socket->abort(); return; }
                    emit login(socket, user, password, session);
                }
                break;
                case GreeterMessages::StartFingerprint: {
                    QString user;
                    Session session;
                    input >> user >> session;
                    if (input.status() == QDataStream::ReadPastEnd) { input.rollbackTransaction(); return; }
                    if (input.status() != QDataStream::Ok) { socket->abort(); return; }
                    emit startFingerprint(socket, user, session);
                }
                break;
                case GreeterMessages::CancelFingerprint: {
                    QString requestId;
                    input >> requestId;
                    if (input.status() == QDataStream::ReadPastEnd) { input.rollbackTransaction(); return; }
                    if (input.status() != QDataStream::Ok || requestId.isEmpty()) { socket->abort(); return; }
                    emit cancelFingerprint(socket, requestId);
                }
                break;
                case GreeterMessages::PowerOff: {
                    // log message
                    qDebug() << "Message received from greeter: PowerOff";

                    // power off
                    daemonApp->powerManager()->powerOff();
                }
                break;
                case GreeterMessages::Reboot: {
                    // log message
                    qDebug() << "Message received from greeter: Reboot";

                    // reboot
                    daemonApp->powerManager()->reboot();
                }
                break;
                case GreeterMessages::Suspend: {
                    // log message
                    qDebug() << "Message received from greeter: Suspend";

                    // suspend
                    daemonApp->powerManager()->suspend();
                }
                break;
                case GreeterMessages::Hibernate: {
                    // log message
                    qDebug() << "Message received from greeter: Hibernate";

                    // hibernate
                    daemonApp->powerManager()->hibernate();
                }
                break;
                case GreeterMessages::HybridSleep: {
                    // log message
                    qDebug() << "Message received from greeter: HybridSleep";
                    // hybrid sleep
                    daemonApp->powerManager()->hybridSleep();
                }
                break;
                default: {
                    // log message
                    qWarning() << "Unknown message" << message;
                }
            }
            if (!input.commitTransaction())
                return;
        }

    }

    void SocketServer::loginFailed(QLocalSocket *socket) {
        SocketWriter(socket) << quint32(DaemonMessages::LoginFailed);
    }

    void SocketServer::loginSucceeded(QLocalSocket *socket) {
        SocketWriter(socket) << quint32(DaemonMessages::LoginSucceeded);
    }

    void SocketServer::informationMessage(QLocalSocket *socket, const QString &message) {
        SocketWriter(socket) << quint32(DaemonMessages::InformationMessage) << message;
    }

    void SocketServer::authenticationCapabilities(QLocalSocket *socket, quint32 capabilities) {
        SocketWriter(socket) << quint32(DaemonMessages::AuthenticationCapabilities) << capabilities;
    }

    void SocketServer::authenticationMethodState(QLocalSocket *socket, const QString &requestId, quint32 state, const QString &detail) {
        SocketWriter(socket) << quint32(DaemonMessages::AuthenticationMethodState) << requestId << state << detail;
    }

    void SocketServer::authenticationAttemptFinished(QLocalSocket *socket, const QString &requestId, bool success, quint32 method) {
        SocketWriter(socket) << quint32(DaemonMessages::AuthenticationAttemptFinished) << requestId << quint32(success) << method;
    }
}
