/***************************************************************************
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

#include <QLocalServer>

namespace SDDM {
    SocketServer::SocketServer(QObject *parent) : QObject(parent) {
    }

    void SocketServer::setSocket(const QString &socket) {
        m_socket = socket;
    }

    bool SocketServer::start() {
        // check flag
        if (m_started)
            return false;

        // log message
        qDebug() << " DAEMON: Socket server starting...";

        // create server
        server = new QLocalServer(this);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        // set server options
        server->setSocketOptions(QLocalServer::UserAccessOption);
#endif

        // start listening
        if (!server->listen(m_socket)) {
            // log message
            qCritical() << " DAEMON: Failed to start socket server.";

            // return fail
            return false;
        }

        // log message
        qDebug() << " DAEMON: Socket server started.";

        // connect signals
        connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void SocketServer::stop() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " DAEMON: Socket server stopping...";

        // delete server
        server->deleteLater();
        server = nullptr;

        // log message
        qDebug() << " DAEMON: Socket server stopped.";
    }

    void SocketServer::newConnection() {
        // get pending connection
        QLocalSocket *socket = server->nextPendingConnection();

        // connect signals
        connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
        connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
    }

    void SocketServer::readyRead() {
        // get app pointer
        DaemonApp *app = qobject_cast<DaemonApp *>(qApp);

        QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());

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
                qDebug() << " DAEMON: Message received from greeter: Connect";

                // send capabilities
                SocketWriter(socket) << quint32(DaemonMessages::Capabilities) << quint32(app->powerManager()->capabilities());

                // send host name
                SocketWriter(socket) << quint32(DaemonMessages::HostName) << app->hostName();
            }
            break;
            case GreeterMessages::Login: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: Login";

                // read username, pasword etc.
                QString user, password, session;
                input >> user >> password >> session;

                // emit signal
                emit login(socket, user, password, session);
            }
            break;
            case GreeterMessages::PowerOff: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: PowerOff";

                // power off
                app->powerManager()->powerOff();
            }
            break;
            case GreeterMessages::Reboot: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: Reboot";

                // reboot
                app->powerManager()->reboot();
            }
            break;
            case GreeterMessages::Suspend: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: Suspend";

                // suspend
                app->powerManager()->suspend();
            }
            break;
            case GreeterMessages::Hibernate: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: Hibernate";

                // hibernate
                app->powerManager()->hibernate();
            }
            break;
            case GreeterMessages::HybridSleep: {
                // log message
                qDebug() << " DAEMON: Message received from greeter: HybridSleep";

                // hybrid sleep
                app->powerManager()->hybridSleep();
            }
            break;
            default: {
                // log message
                qWarning() << " DAEMON: Unknown message" << message;
            }
        }
    }

    void SocketServer::loginFailed(QLocalSocket *socket) {
        SocketWriter(socket) << quint32(DaemonMessages::LoginFailed);
    }

    void SocketServer::loginSucceeded(QLocalSocket *socket) {
        SocketWriter(socket) << quint32(DaemonMessages::LoginSucceeded);
    }
}
