/*
 * Server implementation for X Display Control Protocol
 * Copyright (C) 2013  Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef SDDM_XDMCP_SERVER_H
#define SDDM_XDMCP_SERVER_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtNetwork/QUdpSocket>

// the same as in <X11/Xdmcp.h>
#define XDM_UDP_PORT 177

namespace SDDM {

class Display;
    class DaemonApp;
namespace XDMCP {

    class Server : protected QUdpSocket
    {
        Q_OBJECT
    public:
        /**
         * Get an instance of the XDMCP server. If there isn't any, construct a
         * new one
         *
         * \param parent Parent for the eventual construction
         * \return Singleton XDMCP Server instance
         */
        static Server *instance(DaemonApp *parent = nullptr);
        /**
         * D'tor
         */
        virtual ~Server();

        /**
         * Set port to listen on
         *
         * \param port The port
         */
        void setPort(int port);

        /**
         * Set address to listen on
         *
         * \param address The address
         */
        void setAddress(QHostAddress address);

        /**
         * Start the server
         *
         * \return True if successful
         */
        bool start();

        /**
         * Get server online status
         *
         * \return True if running
         */
        bool isStarted() const;


        /**
         * Returns a new session ID for incoming requests
         */
        uint32_t newSessionId();

        /**
         * Create a new display
         */
        Display *newDisplay(uint32_t sessionId, const QString &hostName, uint32_t displayNumber);
        Display *getDisplay(uint32_t id);
        QString status() const;
        QString hostname() const;

    private slots:
        void newData();
        void socketError(QAbstractSocket::SocketError socketError);
        void removeDisplay(QObject *obj);

    private:
        static Server *self;
        explicit Server(DaemonApp *parent = nullptr);

        QString m_status { "offline" };
        QString m_hostname { "localhost" };
        QHostAddress m_address { QHostAddress::Any };
        quint16 m_port { XDM_UDP_PORT };
        bool m_started { false };
        uint32_t m_lastSession { 0 };
        QMap<int, Display*> m_displays;
        QMap<int, QTimer*> m_timers;
    };
}
}

#endif // SDDM_XDMCP_SERVER_H
