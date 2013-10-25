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

#include "Packet.h"
#include "Server.h"
#include "../Display.h"

namespace SDDM {
namespace XDMCP {

/*******************************************************************************
*                                   PLUMBING
 ******************************************************************************/

    Packet::Packet(const QHostAddress& host, quint16 port)
        : m_host(host)
        , m_port(port)
        , m_valid(true) {

    }

    Packet::Packet(const QHostAddress& host, quint16 port, Reader& r)
        : m_host(host)
        , m_port(port)
        , m_valid(false) {

    }

    Packet::~Packet() {

    }

    bool Packet::isValid() const {
        return m_valid;
    }

    // static
    Packet *Packet::decode(const QByteArray& data, const QHostAddress& host, quint16 port) {
        Reader reader(data);
        uint16_t version, opcode, length;

        reader >> version >> opcode >> length;

        if (version != 1)
            return nullptr;
        if (length != data.size() - 6)
            return nullptr;

        switch (opcode) {
        case _Query:
            return new Query(host, port, reader);
        case _BroadcastQuery:
            return new BroadcastQuery(host, port, reader);
        case _IndirectQuery:
            return new IndirectQuery(host, port, reader);
        case _ForwardQuery:
            return new ForwardQuery(host, port, reader);
        case _Willing:
            return new Willing(host, port, reader);
        case _Unwilling:
            return new Unwilling(host, port, reader);
        case _Request:
            return new Request(host, port, reader);
        case _Accept:
            return new Accept(host, port, reader);
        case _Decline:
            return new Decline(host, port, reader);
        case _Manage:
            return new Manage(host, port, reader);
        case _Refuse:
            return new Refuse(host, port, reader);
        case _Failed:
            return new Failed(host, port, reader);
        case _KeepAlive:
            return new KeepAlive(host, port, reader);
        case _Alive:
            return new Alive(host, port, reader);
        default:
            qDebug() << " XDMCP: Got packet of an unknown type" << opcode;
            return nullptr;
        }
    }

    void Packet::setHost(const QHostAddress host) {
        m_host = QHostAddress(host);
    }

    QHostAddress Packet::host() const {
        return m_host;
    }

    void Packet::setPort(quint16 port) {
        m_port = port;
    }

    quint16 Packet::port() const {
        return m_port;
    }

    QByteArray Packet::encode() const {
        return QByteArray();
    }

    Packet *Packet::onClientReceived() const {
        qDebug() << " XDMCP: Client received a wrong packet type (no action assigned)";
        return nullptr;
    }

    Packet *Packet::onServerReceived() const {
        qDebug() << " XDMCP: Server received a wrong packet type (no action assigned)";
        return nullptr;
    }

    Packet::Query::Query(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_authenticationNames;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Query::encode() const {
        Writer w;
        w << m_authenticationNames;
        return w.finalize(Packet::_Query);
    }

    Packet::BroadcastQuery::BroadcastQuery(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_authenticationNames;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::BroadcastQuery::encode() const {
        Writer w;
        w << m_authenticationNames;
        return w.finalize(Packet::_BroadcastQuery);
    }

    Packet::IndirectQuery::IndirectQuery(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_authenticationNames;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::IndirectQuery::encode() const {
        Writer w;
        w << m_authenticationNames;
        return w.finalize(Packet::_IndirectQuery);
    }

    Packet::ForwardQuery::ForwardQuery(const QHostAddress& host, quint16 port, Reader& r) : Packet(host, port, r) {
        r >> m_clientAddress >> m_clientPort >> m_authenticationNames;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::ForwardQuery::encode() const {
        Writer w;
        w << m_clientAddress << m_clientPort << m_authenticationNames;
        return w.finalize(Packet::_ForwardQuery);
    }

    Packet::Willing::Willing(const QHostAddress& host, quint16 port, const QString& authenticationName, const QString& hostname, const QString& status)
        : Packet(host, port)
        , m_authenticationName(authenticationName.toLatin1())
        , m_hostname(hostname.toLatin1())
        , m_status(status.toLatin1()) {
        qDebug() << " XDMCP: Prepared Willing reply for" << host << port << "with contents" << authenticationName << hostname << status;
    }

    Packet::Willing::Willing(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_authenticationName >> m_hostname >> m_status;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Willing::encode() const {
        Writer w;
        w << m_authenticationName << m_hostname << m_status;
        return w.finalize(Packet::_Willing);
    }

    Packet::Unwilling::Unwilling(const QHostAddress& host, quint16 port, const QString& hostname, const QString& status)
        : Packet(host, port)
        , m_hostname(hostname.toLatin1())
        , m_status(status.toLatin1()) {
        qDebug() << " XDMCP: Prepared Unwilling reply for" << host << port << "with contents" << hostname << status;
    }

    Packet::Unwilling::Unwilling(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_hostname >> m_status;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Unwilling::encode() const {
        Writer w;
        w << m_hostname << m_status;
        return w.finalize(Packet::_Unwilling);
    }

    Packet::Request::Request(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_displayNumber >> m_connectionTypes >> m_connectionAddresses
        >> m_authenticationName >> m_authenticationData >> m_authorizationNames
        >> m_manufacturerDisplayID;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Request::encode() const {
        Writer w;
        w << m_displayNumber << m_connectionTypes << m_connectionAddresses
        << m_authenticationName << m_authenticationData << m_authorizationNames
        << m_manufacturerDisplayID;
        return w.finalize(Packet::_Request);
    }

    Packet::Accept::Accept(const QHostAddress& host, quint16 port, uint32_t sessionId, const QString authenticationName, const QByteArray authenticationData, const QString authorizationName, const QByteArray authorizationData)
        : Packet(host, port)
        , m_sessionID(sessionId)
        , m_authenticationName(authenticationName.toLatin1())
        , m_authenticationData(authenticationData)
        , m_authorizationName(authorizationName.toLatin1())
        , m_authorizationData(authorizationData) {
        qDebug() << " XDMCP: Prepared Accept reply for" << host << port << "with contents" << sessionId << authenticationName << authenticationData << authorizationName << authorizationData;
    }

    Packet::Accept::Accept(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_sessionID >> m_authenticationName >> m_authenticationData
        >> m_authorizationName >> m_authorizationData;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Accept::encode() const {
        Writer w;
        w << m_sessionID << m_authenticationName << m_authenticationData
        << m_authorizationName << m_authorizationData;
        return w.finalize(Packet::_Accept);
    }

    Packet::Decline::Decline(const QHostAddress& host, quint16 port, const QString status, const QString authenticationName, const QByteArray authenticationData)
        : Packet(host, port)
        , m_status(status.toLatin1())
        , m_authenticationName(authenticationName.toLatin1())
        , m_authenticationData(authenticationData) {
        qDebug() << " XDMCP: Prepared Decline reply for" << host << port << "with contents" << status << authenticationName << authenticationData;
    }

    Packet::Decline::Decline(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_status >> m_authenticationName >> m_authenticationData;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Decline::encode() const {
        Writer w;
        w << m_status << m_authenticationName << m_authenticationData;
        return w.finalize(Packet::_Decline);
    }

    Packet::Manage::Manage(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_sessionID >> m_displayNumber >> m_displayClass;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Manage::encode() const {
        Writer w;
        w << m_sessionID << m_displayNumber << m_displayClass;
        return w.finalize(Packet::_Manage);
    }

    Packet::Refuse::Refuse(const QHostAddress& host, quint16 port, uint32_t sessionID)
        : Packet(host, port)
        , m_sessionID(sessionID) {
        qDebug() << " XDMCP: Prepared Refuse reply for" << host << port << "with contents" << sessionID;
    }

    Packet::Refuse::Refuse(const QHostAddress& host, quint16 port, Reader& r) : Packet(host, port, r) {
        r >> m_sessionID;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Refuse::encode() const {
        Writer w;
        w << m_sessionID;
        return w.finalize(Packet::_Refuse);
    }

    Packet::Failed::Failed(const QHostAddress& host, quint16 port, uint32_t sessionID, const QString& status)
        : Packet(host, port)
        , m_sessionID(sessionID)
        , m_status(status.toLatin1()) {
        qDebug() << " XDMCP: Prepared Failed reply for" << host << port << "with contents" << sessionID << status;
    }

    Packet::Failed::Failed(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_sessionID >> m_status;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Failed::encode() const {
        Writer w;
        w << m_sessionID << m_status;
        return w.finalize(Packet::_Failed);
    }

    Packet::KeepAlive::KeepAlive(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_displayNumber >> m_sessionID;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::KeepAlive::encode() const {
        Writer w;
        w << m_displayNumber << m_sessionID;
        return w.finalize(Packet::_KeepAlive);
    }

    Packet::Alive::Alive(const QHostAddress& host, quint16 port, uint8_t sessionRunning, uint32_t sessionID)
        : Packet(host, port)
        , m_sessionRunning(sessionRunning)
        , m_sessionID(sessionID) {
        qDebug() << " XDMCP: Prepared Alive reply for" << host << port << "with contents" << sessionRunning << sessionID;
    }

    Packet::Alive::Alive(const QHostAddress& host, quint16 port, Reader& r)
        : Packet(host, port, r) {
        r >> m_sessionRunning >> m_sessionID;
        if (r.isFinished()) {
            m_valid = true;
        }
    }

    QByteArray Packet::Alive::encode() const {
        Writer w;
        w << m_sessionRunning << m_sessionID;
        return w.finalize(Packet::_Alive);
    }

/*******************************************************************************
 *                            SERVER IMPLEMENTATIONS
 ******************************************************************************/

    Packet *Packet::Query::onServerReceived() const {
        if (m_authenticationNames.isEmpty()) {
            return new Willing(m_host, m_port, "", Server::instance()->hostname(), Server::instance()->status());
        }
        else {
            return new Unwilling(m_host, m_port, Server::instance()->hostname(), "Server does not support authentication");
        }
    }

    Packet* Packet::Request::onServerReceived() const {
        qDebug() << " XDMCP: Server: Received Request" << m_displayNumber << m_connectionTypes << m_connectionAddresses << m_authenticationName << m_authenticationData << m_authorizationNames << m_manufacturerDisplayID;
        if (m_authorizationNames.contains("MIT-MAGIC-COOKIE-1")) {
            uint32_t sessionId = Server::instance()->newSessionId();
            QHostAddress addr(QString("%1.%2.%3.%4").arg((uint) m_connectionAddresses.first()[0]).arg((uint) m_connectionAddresses.first()[1]).arg((uint) m_connectionAddresses.first()[2]).arg((uint) m_connectionAddresses.first()[3]));
            Display *display = Server::instance()->newDisplay(sessionId, addr.toString(), m_displayNumber);
            return new Accept(m_host, m_port, sessionId, m_authenticationName, m_authenticationData, "MIT-MAGIC-COOKIE-1", display->rawCookie());
        } else {
            return new Decline(m_host, m_port, Server::instance()->status(), m_authenticationName, m_authenticationData);
        }
    }

    Packet* Packet::Manage::onServerReceived() const {
        Display *display = Server::instance()->getDisplay(m_sessionID);
        if (display != nullptr) {
            display->start();
            return nullptr;
        } else {
            return new Refuse(m_host, m_port, m_sessionID);
        }
    }

    Packet* Packet::KeepAlive::onServerReceived() const {
        Display *display = Server::instance()->getDisplay(m_sessionID);
        if (display == nullptr)
            return new Alive(m_host, m_port, 0, m_sessionID);
        else if (display->displayId() != m_displayNumber)
            return new Alive(m_host, m_port, 0, m_sessionID);
        else {
            return new Alive(m_host, m_port, 1, m_sessionID);
        }
    }

/*******************************************************************************
 *                            CLIENT IMPLEMENTATIONS
 ******************************************************************************/

};
};
