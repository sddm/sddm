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

#ifndef SDE_DISPLAY_H
#define SDE_DISPLAY_H

#include <QObject>

class QLocalSocket;

namespace SDE {
    enum class Capabilities;

    class Authenticator;
    class DisplayServer;
    class SocketServer;
    class Greeter;

    class Display : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Display)
    public:
        explicit Display(const QString &display, QObject *parent = 0);
        ~Display();

        const QString &name() const;

    public slots:
        void start();
        void stop();

        void capabilities(QLocalSocket *socket);

        void login(QLocalSocket *socket, const QString &user, const QString &password, const QString &session);

        void powerOff();
        void reboot();

        void suspend();
        void hibernate();
        void hybridSleep();

    signals:
        void capabilities(QLocalSocket *socket, enum Capabilities capabilities);

        void loginFailed(QLocalSocket *socket);
        void loginSucceeded(QLocalSocket *socket);

    private:
        bool m_started { false };

        QString m_display { "" };
        QString m_socket { "" };
        QString m_authPath { "" };

        Authenticator *m_authenticator { nullptr };
        DisplayServer *m_displayServer { nullptr };
        SocketServer *m_socketServer { nullptr };
        Greeter *m_greeter { nullptr };
    };
}

#endif // SDE_DISPLAY_H
