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
        enum DisplayServerType {
            X11DisplayServerType,
            X11UserDisplayServerType,
            WaylandDisplayServerType
        };
        Q_ENUM(DisplayServerType)

        explicit Display(Seat *parent);
        ~Display();

        DisplayServerType displayServerType() const;
        DisplayServer *displayServer() const;

        QString displayId() const;
        const int terminalId() const;

        const QString &name() const;

        QString sessionType() const;

        Seat *seat() const;

    public slots:
        bool start();
        void stop();

        void login(QLocalSocket *socket,
                   const QString &user, const QString &password,
                   const Session &session);
        bool attemptAutologin();
        bool fingerprintLogin();
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

        DisplayServerType m_displayServerType = X11DisplayServerType;

        bool m_relogin { true };
        bool m_started { false };

        int m_terminalId { 7 };

        Session m_lastSession;

        QString m_passPhrase;
        QString m_sessionName;
        QString m_reuseSessionId;

        Auth *m_auth { nullptr };
        DisplayServer *m_displayServer { nullptr };
        Seat *m_seat { nullptr };
        SocketServer *m_socketServer { nullptr };
        QLocalSocket *m_socket { nullptr };
        Greeter *m_greeter { nullptr };

        /*!
         \brief Write utmp/wtmp/btmp records when a user logs in
         \param vt  Virtual terminal (tty7, tty8,...)
         \param displayName  Display (:0, :1,...)
         \param user  User logging in
         \param pid  User process ID (e.g. PID of startkde)
         \param authSuccessful  Was authentication successful
        */
        void utmpLogin(const QString &vt, const QString &displayName, const QString &user, qint64 pid, bool authSuccessful);

        /*!
         \brief Write utmp/wtmp records when a user logs out
         \param vt  Virtual terminal (tty7, tty8,...)
         \param displayName  Display (:0, :1,...)
         \param pid  User process ID (e.g. PID of startkde)
        */
        void utmpLogout(const QString &vt, const QString &displayName, qint64 pid);

    private slots:
        void slotRequestChanged();
        void slotAuthenticationFinished(const QString &user, bool success);
        void slotSessionStarted(bool success, qint64 pid);
        void slotHelperFinished(Auth::HelperExitStatus status);
        void slotAuthInfo(const QString &message, Auth::Info info);
        void slotAuthError(const QString &message, Auth::Error error);
    };
}

#endif // SDDM_DISPLAY_H
