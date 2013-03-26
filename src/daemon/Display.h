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

#ifndef SDDM_DISPLAY_H
#define SDDM_DISPLAY_H

#include <QObject>

class QLocalSocket;

namespace SDDM {
    class Authenticator;
    class DisplayServer;
    class SocketServer;
    class Greeter;

    class Display : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Display)
    public:
        explicit Display(const int displayId, const int terminalId, QObject *parent = 0);
        ~Display();

        const int displayId() const;
        const int terminalId() const;

        const QString &name() const;
        const QString &cookie() const;
        const QString &seatId() const;

    public slots:
        void start();
        void stop();

        void login(QLocalSocket *socket, const QString &user, const QString &password, const QString &session);

    signals:
        void loginFailed(QLocalSocket *socket);
        void loginSucceeded(QLocalSocket *socket);

    private:
        bool m_relogin { true };
        bool m_started { false };

        int m_displayId { 0 };
        int m_terminalId { 7 };

        QString m_display { ":0" };
        QString m_cookie { "" };
        QString m_socket { "" };
        QString m_authPath { "" };

        Authenticator *m_authenticator { nullptr };
        DisplayServer *m_displayServer { nullptr };
        SocketServer *m_socketServer { nullptr };
        Greeter *m_greeter { nullptr };
    };
}

#endif // SDDM_DISPLAY_H
