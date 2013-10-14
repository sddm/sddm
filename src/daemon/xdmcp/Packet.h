/*
 * Packet type handling for X Display Control Protocol
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

#ifndef SDDM_XDMCP_PACKET_H
#define SDDM_XDMCP_PACKET_H

#include <QtCore/QByteArray>
#include <QtNetwork/QHostAddress>

#include "Utils.h"

namespace SDDM {
namespace XDMCP {
    class Reader;
    /**
    * XDMCP Packet main class
    *
    * Defines interface and static methods to work with the protocol data
    */
    class Packet {
    public:
        virtual ~Packet();

        /**
        * Get the packet's validity (especially for responses)
        *
        * \return validity
        */
        bool isValid() const;

        /**
        * Defines server behavior on receiving this packet
        */
        virtual Packet *onServerReceived() const;

        /**
        * Defines client behavior on receiving this packet
        */
        virtual Packet *onClientReceived() const;

        /**
        * Encode the packet to raw data according to the protocol
        *
        * \return Data byte array
        */
        virtual QByteArray encode() const;

        /**
        * Decode raw packet data and create a packet for further processing
        *
        * \param data Raw data from the socket
        * \param host Source host of the packet
        * \param port Source port of the packet
        * \return Parsed packet
        */
        static Packet *decode(const QByteArray& data, const QHostAddress& host = QHostAddress(), quint16 port = 0);

        /**
         * Set the packet's source/destination host
         *
         * \param host The host
         */
        void setHost(const QHostAddress host);

        /**
         * Get the packet's source/destination host
         *
         * \return The host
         */
        QHostAddress host() const;

        /**
         * Set the packet's source/destination host
         *
         * \param port The port
         */
        void setPort(quint16 port);

        /**
         * Get the packet's source/destination host
         *
         * \return The port
         */
        quint16 port() const;

        /**
         * Redundancy for everyone!
         */
        enum Opcode {
            _None = 0,
            _BroadcastQuery = 1,
            _Query,
            _IndirectQuery,
            _ForwardQuery,
            _Willing,
            _Unwilling,
            _Request,
            _Accept,
            _Decline,
            _Manage,
            _Refuse,
            _Failed,
            _KeepAlive,
            _Alive,
        };

        class BroadcastQuery;
        class Query;
        class IndirectQuery;
        class ForwardQuery;
        class Willing;
        class Unwilling;
        class Request;
        class Accept;
        class Decline;
        class Manage;
        class Refuse;
        class Failed;
        class KeepAlive;
        class Alive;

    protected:
        /**
         * C'tor targetted for creating response packets
         *
         * Automatically sets the packet to be valid
         * \param host Destination host for the response
         * \param port Destination port for the response
         */
        Packet(const QHostAddress& host, quint16 port);
        /**
         * C'tor targetted for parsing raw data
         *
         * \param host Destination host for the response
         * \param port Destination port for the response
         * \param r Reader containing the packet's raw data
         */
        Packet(const QHostAddress& host, quint16 port, Reader& r);

        QHostAddress m_host;
        quint16 m_port { 0 };
        bool m_valid { false };
    };

    class Packet::BroadcastQuery : public Packet {
    public:
        BroadcastQuery(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        QVector<QByteArray> m_authenticationNames;
    };

    class Packet::Query : public Packet {
    public:
        Query(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Server side handling of Query packet
         *
         * Client sends a list of authorization names
         *
         * If the list is empty (and we are willing to continue without
         * authorization), we reply with \ref Willing with empty
         * authenticationName
         *
         * Otherwise, we choose the one name that complies to the supported ones
         * in the server and use it as authenticationName for the \ref Willing
         * packet
         *
         * If none of the names complies and/or we don't want to continue 
         * without authorization, the reply is \ref Unwilling
         *
         * \return Response
         */
        virtual Packet *onServerReceived() const;
    private:
        QVector<QByteArray> m_authenticationNames;
    };

    class Packet::IndirectQuery : public Packet {
    public:
        IndirectQuery(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        QVector<QByteArray> m_authenticationNames;
    };

    class Packet::ForwardQuery : public Packet {
    public:
        ForwardQuery(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        QByteArray m_clientAddress;
        QByteArray m_clientPort;
        QVector<QByteArray> m_authenticationNames;
    };

    class Packet::Willing : public Packet {
    public:
        Willing(const QHostAddress& host, quint16 port, 
                const QString& authenticationName, const QString& hostname,
                const QString& status);
        Willing(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Client side handling of Willing packet
         *
         * Description TBD
         *
         * Reply on success is \ref Request
         *
         * \return Response
         */
//         virtual Packet *onClientReceived() const;
    private:
        QByteArray m_authenticationName;
        QByteArray m_hostname;
        QByteArray m_status;
    };

    class Packet::Unwilling : public Packet {
    public:
        Unwilling(const QHostAddress& host, quint16 port,
                  const QString& hostname, const QString& status);
        Unwilling(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        QByteArray m_hostname;
        QByteArray m_status;
    };

    class Packet::Request : public Packet {
    public:
        Request(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Server side handling of Request packet
         *
         * Client informs there will be displey displayNumber running on
         * connectionAddresses accessible via connectionTypes.
         * It also authorizes with the authenticationName and authenticationData.
         * It sends a list of propsed authorizationNames for the server to choose
         * one of them and reply using the \ref Accept packet where the chosen
         * authorizationName will be stored along with the authorizationData for
         * the client and the new sessionID.
         *
         * If the display cannot be used, the server replies using the \ref Decline
         * packet with its status.
         *
         * \return Response
         */
        virtual Packet *onServerReceived() const;
    private:
        uint16_t m_displayNumber;
        QVector<uint16_t> m_connectionTypes;
        QVector<QByteArray> m_connectionAddresses;
        QByteArray m_authenticationName;
        QByteArray m_authenticationData;
        QVector<QByteArray> m_authorizationNames;
        QByteArray m_manufacturerDisplayID;
    };

    class Packet::Accept : public Packet {
    public:
        Accept(const QHostAddress& host, quint16 port, uint32_t sessionId,
               const QString authenticationName, const QByteArray authenticationData,
               const QString authorizationName, const QByteArray authorizationData);
        Accept(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Client side handling of Accept packet
         *
         * Description TBD
         *
         * Reply on succes is \ref Manage
         *
         * \return Response
         */
//         virtual Packet *onClientReceived() const;
    private:
        uint32_t m_sessionID;
        QByteArray m_authenticationName;
        QByteArray m_authenticationData;
        QByteArray m_authorizationName;
        QByteArray m_authorizationData;
    };

    class Packet::Decline : public Packet {
    public:
        Decline(const QHostAddress& host, quint16 port, const QString status,
                const QString authenticationName, const QByteArray authenticationData);
        Decline(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        QByteArray m_status;
        QByteArray m_authenticationName;
        QByteArray m_authenticationData;
    };

    class Packet::Manage : public Packet {
    public:
        Manage(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Server side handling of Manage packet
         *
         * Client asks the server to open a connection to its opened display
         * specified in the previous Request packet.
         *
         * There is no answer on success, just opening a connection.
         *
         * If the connection is specified wrong (erroneous sessionID, etc.), then
         * the server replies with a \ref Refuse packet.
         *
         * If the connection cannot be opened due to an internal error, 
         * \ref Failed packet is sent to the client.
         *
         * \return Response
         */
        virtual Packet *onServerReceived() const;
    private:
        uint32_t m_sessionID;
        uint16_t m_displayNumber;
        QByteArray m_displayClass;
    };

    class Packet::Refuse : public Packet {
    public:
        Refuse(const QHostAddress& host, quint16 port, uint32_t sessionID);
        Refuse(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        uint32_t m_sessionID;
    };

    class Packet::Failed : public Packet {
    public:
        Failed(const QHostAddress& host, quint16 port, uint32_t sessionID, const QString& status);
        Failed(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        uint32_t m_sessionID;
        QByteArray m_status;
    };

    class Packet::KeepAlive : public Packet {
    public:
        KeepAlive(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
        /**
         * Server side handling of KeepAlive packet
         *
         * Clients asks the server if the session is still alive.
         *
         * Server replies with \ref Alive packet with either sessionRunning == 0
         * for a dead session or sessionRunning != 0 for a live one
         */
        virtual Packet *onServerReceived() const;
    private:
        uint16_t m_displayNumber;
        uint32_t m_sessionID;
    };

    class Packet::Alive : public Packet {
    public:
        Alive(const QHostAddress& host, quint16 port, uint8_t sessionRunning, uint32_t sessionID);
        Alive(const QHostAddress& host, quint16 port, Reader& r);
        virtual QByteArray encode() const;
    private:
        uint8_t m_sessionRunning;
        uint32_t m_sessionID;
    };

} // namespace XDMCP
} // namespace SDDM

#endif // SDDM_XDMCP_PACKET_H
