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

#include "Server.h"
#include "Packet.h"
#include "../DaemonApp.h"
#include "../Display.h"

#include <QtNetwork/QHostInfo>

namespace SDDM {
namespace XDMCP {

    Server *Server::self = nullptr;

    Server* Server::instance(DaemonApp* parent) {
        if (self == nullptr) {
            self = new Server(parent);
        }
        return self;
    }

    Server::Server(DaemonApp* parent)
        : QUdpSocket(parent)
        , m_hostname(QHostInfo::localHostName()) {

    }

    Server::~Server() {

    }

    bool Server::start() {
        qDebug() << " XDMCP: Server: Starting...";
        connect(this, SIGNAL(readyRead()), this, SLOT(newData()));
        bool result = bind(m_address, m_port);
        if (!result) {
            qDebug() << " XDMCP: Server: Cannot bind" << m_address << m_port << errorString();
        }
        else {
            m_started = true;
            m_status = "online";
            qDebug() << " XDMCP: Server: Started and listening on" << m_address << ":" << m_port;
        }
        return result;
    }

    void Server::socketError(QAbstractSocket::SocketError socketError) {
        qDebug() << " XDMCP: Error:" << errorString();
        // TODO: error recovery
        m_started = false;
        m_status = "error";
    }

    QString Server::hostname() const {
        return m_hostname;
    }

    QString Server::status() const {
        return m_status;
    }

    bool Server::isStarted() const {
        return m_started;
    }

    uint32_t Server::newSessionId() {
        // realistically, can this serve more than 4 billion clients to actually cause trouble in removeDisplay?
        while (m_displays.keys().contains(m_lastSession))
            m_lastSession++;
        return m_lastSession++;
    }

    Display* Server::newDisplay(uint32_t sessionId, QString hostName, uint32_t displayNumber) {
        if (m_displays.contains(sessionId))
            return nullptr;
        Display *display = new Display(hostName, displayNumber, this);
        connect(display, SIGNAL(destroyed(QObject*)), SLOT(removeDisplay(QObject*)));
        m_displays[sessionId] = display;
        return display;
    }

    Display* Server::getDisplay(uint32_t id) {
        if (m_displays.contains(id))
            return m_displays[id];
        else
            return nullptr;
    }

    void Server::removeDisplay(QObject* obj) {
        int key = m_displays.key(qobject_cast<Display*>(obj), -1);
        if (key == -1)
            return;

        m_displays.remove(key);
    }

    void Server::setAddress(QHostAddress address) {
        m_address = address;
    }

    void Server::setPort(int port) {
        m_port = port;
    }

    void Server::newData() {
        while (hasPendingDatagrams()) {
            QByteArray data;
            QHostAddress sender;
            quint16 port;
            data.resize(pendingDatagramSize());

            readDatagram(data.data(), data.size(), &sender, &port);

            Packet *toProcess = Packet::decode(data, sender, port);
            if (toProcess && toProcess->isValid()) {
                Packet *response = toProcess->onServerReceived();
                if (response && response->isValid()) {
                    writeDatagram(response->encode(), response->host(), response->port());
                }
                delete response;
            } else {
                qDebug() << " XDMCP: Server: Received packet wasn't decoded as valid";
            }
            delete toProcess;
        }
    }

} // namespace XDMCP
} // namespace SDDM

#include "Server.moc"
