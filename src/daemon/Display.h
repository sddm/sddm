/***************************************************************************
* Copyright (c) 2014-2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2014 Martin Bříza <mbriza@redhat.com>
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

#ifndef SDDM_DISPLAY_H
#define SDDM_DISPLAY_H

#include <QObject>
#include <QDir>

#include "Auth.h"
#include "Session.h"

class QLocalSocket;

namespace SDDM {
    class Authenticator;
    class DisplayServer;
    class Seat;
    class SocketServer;
    class Greeter;

    class Display : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Display)
    public:
        explicit Display(int terminalId, Seat *parent);
        ~Display();

        QString displayId() const;
        const int terminalId() const;

        const QString &name() const;

        QString sessionType() const;

        Seat *seat() const;

    public slots:
        void start();
        void stop();

        void login(QLocalSocket *socket,
                   const QString &user, const QString &password,
                   const Session &session);
        bool attemptAutologin();
        void displayServerStarted();

    signals:
        void stopped();

        void loginFailed(QLocalSocket *socket);
        void loginSucceeded(QLocalSocket *socket);

    private:
        QString findGreeterTheme() const;
        bool findSessionEntry(const QDir &dir, const QString &name) const;

        void startAuth(const QString &user, const QString &password,
                       const Session &session);

        bool m_relogin { true };
        bool m_started { false };

        int m_terminalId { 7 };

        Session m_lastSession;

        QString m_passPhrase;
        QString m_sessionName;

        Auth *m_auth { nullptr };
        DisplayServer *m_displayServer { nullptr };
        Seat *m_seat { nullptr };
        SocketServer *m_socketServer { nullptr };
        QLocalSocket *m_socket { nullptr };
        Greeter *m_greeter { nullptr };

    private slots:
        void slotRequestChanged();
        void slotAuthenticationFinished(const QString &user, bool success);
        void slotSessionStarted(bool success);
        void slotHelperFinished(Auth::HelperExitStatus status);
        void slotAuthInfo(const QString &message, Auth::Info info);
        void slotAuthError(const QString &message, Auth::Error error);
    };
}

#endif // SDDM_DISPLAY_H
