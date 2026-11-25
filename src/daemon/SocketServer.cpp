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
#include <cstring>

namespace SDDM {
    namespace {
        void wipeSecretString(QString &secret) {
            if (!secret.isEmpty()) {
                secret.detach();
                explicit_bzero(secret.data(), size_t(secret.size()) * sizeof(QChar));
                secret.clear();
            }
        }
    }
    SocketServer::SocketServer(QObject *parent) : QObject(parent) {
    }

    QString SocketServer::socketAddress() const {
        if (m_server)
            return m_server->fullServerName();
        return QString();
    }
    bool SocketServer::isCorrelated(QLocalSocket *socket) const {
        return m_correlatedSockets.contains(socket);
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
        m_activeSockets.clear();
        m_correlatedSockets.clear();
        m_queriedSockets.clear();
        m_legacyClearSockets.clear();
        m_server->deleteLater();
        m_server = nullptr;

        // log message
        qDebug() << "Socket server stopped.";
    }

    void SocketServer::newConnection() {
        constexpr int maxActiveConnections = 16;
        while (m_server && m_server->hasPendingConnections()) {
            QLocalSocket *socket = m_server->nextPendingConnection();
            if (!socket)
                continue;
            if (m_activeSockets.size() >= maxActiveConnections) {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            m_activeSockets.insert(socket);
            socket->setReadBufferSize(1024 * 1024);

            connect(socket, &QLocalSocket::readyRead, this, &SocketServer::readyRead);
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
                m_activeSockets.remove(socket);
                m_correlatedSockets.remove(socket);
                m_queriedSockets.remove(socket);
                m_legacyClearSockets.remove(socket);
                emit disconnected(socket);
                socket->deleteLater();
            });
        }
    }

    void SocketServer::readyRead() {
        auto *socket = qobject_cast<QLocalSocket *>(sender());
        if (!socket)
            return;
        QDataStream input(socket);
        constexpr qint64 inputLimit = 1024 * 1024;
        if (socket->bytesAvailable() >= inputLimit) {
            socket->abort();
            return;
        }
        while (socket->bytesAvailable() >= qint64(sizeof(quint32))) {
            quint32 value = 0, hasContext = 0, method = 0, sessionType = 0;
            quint64 serial = 0;
            QString user, password, requestId, sessionFile;
            Session session;
            input.startTransaction();
            input >> value;
            const auto message = GreeterMessages(value);
            switch (message) {
            case GreeterMessages::SetAuthenticationContext:
                input >> serial >> hasContext;
                if (hasContext == 1)
                    input >> user >> sessionType >> sessionFile;
                break;
            case GreeterMessages::Login:
                input >> user >> password >> sessionType >> sessionFile;
                break;
            case GreeterMessages::StartAuthenticationMethod:
                input >> serial >> method;
                break;
            case GreeterMessages::CancelAuthenticationMethod:
                input >> serial >> requestId >> method;
                break;
            case GreeterMessages::Connect:
            case GreeterMessages::QueryAuthenticationCapabilities:
            case GreeterMessages::PowerOff:
            case GreeterMessages::Reboot:
            case GreeterMessages::Suspend:
            case GreeterMessages::Hibernate:
            case GreeterMessages::HybridSleep:
                break;
            default:
                socket->abort();
                return;
            }
            if (!input.commitTransaction()) {
                if (input.status() != QDataStream::ReadPastEnd)
                    socket->abort();
                return;
            }
            const bool contextMessage = message == GreeterMessages::SetAuthenticationContext;
            const bool methodMessage = message == GreeterMessages::StartAuthenticationMethod
                                    || message == GreeterMessages::CancelAuthenticationMethod;
            if ((contextMessage && (!serial || hasContext > 1 || !m_queriedSockets.contains(socket)))
                || (methodMessage && (!serial || method != quint32(AuthenticationMethod::Fingerprint)
                                      || !m_correlatedSockets.contains(socket)))
                || sessionType > quint32(Session::WaylandSession) || sessionFile.size() > 4096
                || user.size() > 256 || password.size() > 65536 || requestId.size() > 64) {
                socket->abort();
                return;
            }
            if (message == GreeterMessages::Login || (contextMessage && hasContext))
                session.setTo(Session::Type(sessionType), sessionFile);
            // Dispatch only after the complete transaction has been committed.
            switch (message) {
            case GreeterMessages::Connect:
                if (daemonApp && daemonApp->powerManager()) {
                    SocketWriter(socket) << quint32(DaemonMessages::Capabilities)
                                         << quint32(daemonApp->powerManager()->capabilities());
                    SocketWriter(socket) << quint32(DaemonMessages::HostName) << daemonApp->hostName();
                }
                emit connected();
                break;
            case GreeterMessages::QueryAuthenticationCapabilities:
                m_queriedSockets.insert(socket);
                SocketWriter(socket) << quint32(DaemonMessages::AuthenticationCapabilities) << quint32(0x2);
                break;
            case GreeterMessages::SetAuthenticationContext:
                // Stay correlated through a pending/rejected clear barrier.
                if (!hasContext && !m_correlatedSockets.contains(socket))
                    m_legacyClearSockets.insert(socket);
                m_correlatedSockets.insert(socket);
                emit setAuthenticationContext(socket, serial, hasContext == 1, user, session);
                break;
            case GreeterMessages::Login:
                emit login(socket, user, password, session);
                wipeSecretString(password);
                break;
            case GreeterMessages::StartAuthenticationMethod:
                emit startFingerprint(socket, serial);
                break;
            case GreeterMessages::CancelAuthenticationMethod:
                emit cancelFingerprint(socket, serial, requestId);
                break;
            case GreeterMessages::PowerOff:
            case GreeterMessages::Reboot:
            case GreeterMessages::Suspend:
            case GreeterMessages::Hibernate:
            case GreeterMessages::HybridSleep:
                emit powerRequested(message);
                break;
            default:
                Q_UNREACHABLE();
            }
            if (socket->state() != QLocalSocket::ConnectedState)
                return;
        }
    }

    void SocketServer::loginFailed(QLocalSocket *socket) {
        if (socket && (!m_correlatedSockets.contains(socket) || m_legacyClearSockets.contains(socket)))
            SocketWriter(socket) << quint32(DaemonMessages::LoginFailed);
    }

    void SocketServer::loginSucceeded(QLocalSocket *socket) {
        // The real legacy attempt may finish while its clear barrier is pending.
        // GreeterProxy retains that completion until an exact committed rejection;
        // an accepted clear discards it. Intermediate/raw feedback stays suppressed.
        if (socket && (!m_correlatedSockets.contains(socket) || m_legacyClearSockets.contains(socket)))
            SocketWriter(socket) << quint32(DaemonMessages::LoginSucceeded);
    }

    void SocketServer::informationMessage(QLocalSocket *socket, const QString &message) {
        if (socket && !m_correlatedSockets.contains(socket))
            SocketWriter(socket) << quint32(DaemonMessages::InformationMessage) << message;
    }

    void SocketServer::authenticationContext(QLocalSocket *socket, quint64 serial, quint32 accepted,
                                             const QString &requestId, const QString &user, const Session &session,
                                             quint32 availability, quint32 phase, const QString &detailCode) {
        if (!m_correlatedSockets.contains(socket))
            return;
        SocketWriter(socket) << quint32(DaemonMessages::AuthenticationContext) << serial << accepted
                             << requestId << user << session << availability << phase << detailCode;
        if ((accepted == 1 && user.isEmpty() && detailCode == QLatin1String("cleared"))
            || (accepted == 0 && m_legacyClearSockets.contains(socket))) {
            m_correlatedSockets.remove(socket);
            m_legacyClearSockets.remove(socket);
        }
    }

    void SocketServer::authenticationMethodState(QLocalSocket *socket, quint64 serial, const QString &requestId,
                                                  quint32 method, const QString &user, const Session &session,
                                                  quint32 state, quint32 phase, const QString &detailCode) {
        if (m_correlatedSockets.contains(socket))
            SocketWriter(socket) << quint32(DaemonMessages::AuthenticationMethodState) << serial
                                 << requestId << method << user << session << state << phase << detailCode;
    }

    void SocketServer::authenticationAttemptFinished(QLocalSocket *socket, quint64 serial, const QString &requestId,
                                                      const QString &user, const Session &session,
                                                      quint32 result, quint32 winningMethod) {
        if (m_correlatedSockets.contains(socket))
            SocketWriter(socket) << quint32(DaemonMessages::AuthenticationAttemptFinished) << serial
                                 << requestId << user << session << result << winningMethod;
    }
}
