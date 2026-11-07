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

    void SocketServer::conversationAuthorized(QLocalSocket *socket, quint32 attempt) {
        if (socket) SocketWriter(socket) << quint32(DaemonMessages::ConversationAuthorized) << attempt;
    }
    void SocketServer::conversationPrompt(QLocalSocket *socket, quint32 attempt, quint32 prompt, const QString &message, bool hidden) {
        if (socket) SocketWriter(socket) << quint32(DaemonMessages::ConversationPrompt) << attempt << prompt << message << quint32(hidden);
    }
    void SocketServer::conversationMessage(QLocalSocket *socket, quint32 attempt, const QString &message, bool error) {
        if (socket) SocketWriter(socket) << quint32(DaemonMessages::ConversationMessage) << attempt << message << quint32(error);
    }
    void SocketServer::conversationFinished(QLocalSocket *socket, quint32 attempt, quint32 result) {
        if (socket) SocketWriter(socket) << quint32(DaemonMessages::ConversationFinished) << attempt << result;
    }

    void SocketServer::newConnection() {
        // get pending connection
        QLocalSocket *socket = m_server->nextPendingConnection();

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
        while(socket->bytesAvailable()) {
            if (socket->bytesAvailable() < 4) return;
            input.startTransaction();
            // read message
            quint32 message;
            input >> message;

            switch (GreeterMessages(message)) {
                case GreeterMessages::Connect: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: Connect";

                    // send capabilities
                    SocketWriter(socket) << quint32(DaemonMessages::Capabilities) << quint32(daemonApp->powerManager()->capabilities());

                    // send host name
                    SocketWriter(socket) << quint32(DaemonMessages::HostName) << daemonApp->hostName();

                    // emit signal
                    emit connected();
                }
                break;
                case GreeterMessages::ConversationHello: {
                    if (!input.commitTransaction()) return;
                    SocketWriter(socket) << quint32(DaemonMessages::ConversationVersion) << quint32(1);
                    break;
                }
                case GreeterMessages::Login: {
                    // log message
                    qDebug() << "Message received from greeter: Login";

                    // read username, pasword etc.
                    QString user, password, filename;
                    Session session;
                    input >> user >> password >> session;
                    if (!input.commitTransaction()) return;

                    // emit signal
                    emit login(socket, user, password, session);
                    password.fill(QChar(0));
                }
                break;
                case GreeterMessages::BeginConversation: {
                    quint32 attempt = 0; QString user; Session session;
                    input >> attempt >> user >> session;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok)
                        emit conversationBegin(socket, attempt, user, session);
                    break;
                }
                case GreeterMessages::ConversationResponse: {
                    quint32 attempt = 0, prompt = 0; QString response;
                    input >> attempt >> prompt >> response;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok && response.size() <= 4096)
                        emit conversationResponse(socket, attempt, prompt, response);
                    response.fill(QChar(0));
                    break;
                }
                case GreeterMessages::CancelConversation: {
                    quint32 attempt = 0; input >> attempt;
                    if (!input.commitTransaction()) return;
                    if (input.status() == QDataStream::Ok) emit conversationCancel(socket, attempt);
                    break;
                }
                case GreeterMessages::PowerOff: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: PowerOff";

                    // power off
                    daemonApp->powerManager()->powerOff();
                }
                break;
                case GreeterMessages::Reboot: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: Reboot";

                    // reboot
                    daemonApp->powerManager()->reboot();
                }
                break;
                case GreeterMessages::Suspend: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: Suspend";

                    // suspend
                    daemonApp->powerManager()->suspend();
                }
                break;
                case GreeterMessages::Hibernate: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: Hibernate";

                    // hibernate
                    daemonApp->powerManager()->hibernate();
                }
                break;
                case GreeterMessages::HybridSleep: {
                    if (!input.commitTransaction()) return;
                    // log message
                    qDebug() << "Message received from greeter: HybridSleep";
                    // hybrid sleep
                    daemonApp->powerManager()->hybridSleep();
                }
                break;
                default: {
                    // log message
                    input.abortTransaction();
                    qWarning() << "Unknown socket message" << message;
                    socket->disconnectFromServer();
                    return;
                }
            }
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
}
