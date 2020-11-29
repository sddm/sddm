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

#include "DisplayManager.h"

#include "DaemonApp.h"
#include "SeatManager.h"

#include "displaymanageradaptor.h"
#include "seatadaptor.h"
#include "sessionadaptor.h"

const QString DISPLAYMANAGER_SERVICE = QStringLiteral("org.freedesktop.DisplayManager");
const QString DISPLAYMANAGER_PATH = QStringLiteral("/org/freedesktop/DisplayManager");
const QString DISPLAYMANAGER_SEAT_PATH = QStringLiteral("/org/freedesktop/DisplayManager/Seat");
const QString DISPLAYMANAGER_SESSION_PATH = QStringLiteral("/org/freedesktop/DisplayManager/Session");

namespace SDDM {
    DisplayManager::DisplayManager(QObject *parent) : QObject(parent) {
        // create adaptor
        new DisplayManagerAdaptor(this);

        // register object
        QDBusConnection connection = (daemonApp->testing()) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService(DISPLAYMANAGER_SERVICE);
        connection.registerObject(DISPLAYMANAGER_PATH, this);
    }

    QString DisplayManager::seatPath(const QString &seatName) {
        return DISPLAYMANAGER_SEAT_PATH + seatName.mid(4);
    }

    QString DisplayManager::sessionPath(const QString &sessionName) {
        return DISPLAYMANAGER_SESSION_PATH + sessionName.mid(7);
    }

    ObjectPathList DisplayManager::Seats() const {
        ObjectPathList seats;

        for (DisplayManagerSeat *seat: m_seats)
            seats << ObjectPath(seat->Path());

        return seats;
    }

    ObjectPathList DisplayManager::Sessions(DisplayManagerSeat *seat) const {
        ObjectPathList sessions;

        for (DisplayManagerSession *session: m_sessions)
            if (seat == nullptr || seat->Name() == session->Seat())
                sessions << ObjectPath(session->Path());

        return sessions;
    }

    void DisplayManager::AddSeat(const QString &name) {
        // create seat object
        DisplayManagerSeat *seat = new DisplayManagerSeat(name, this);

        // add to the list
        m_seats << seat;

        // emit signal
        emit SeatAdded(ObjectPath(seat->Path()));
    }

    void DisplayManager::RemoveSeat(const QString &name) {
        // find seat
        for (DisplayManagerSeat *seat: m_seats) {
            if (seat->Name() == name) {
                // remove from list
                m_seats.removeAll(seat);

                // get object path
                ObjectPath path = ObjectPath(seat->Path());

                // delete seat
                seat->deleteLater();

                // emit signal
                emit SeatRemoved(path);
            }
        }
    }

    void DisplayManager::AddSession(const QString &name, const QString &seat, const QString &user) {
        // create session object
        DisplayManagerSession *session = new DisplayManagerSession(name, seat, user, this);

        // add to the list
        m_sessions << session;

        // emit signal
        emit SessionAdded(ObjectPath(session->Path()));
    }

    void DisplayManager::RemoveSession(const QString &name) {
        // find session
        for (DisplayManagerSession *session: m_sessions) {
            if (session->Name() == name) {
                // remove from list
                m_sessions.removeAll(session);

                // get object path
                ObjectPath path = ObjectPath(session->Path());

                // delete session
                session->deleteLater();

                // emit signal
                emit SessionRemoved(path);
            }
        }
    }

    DisplayManagerSeat::DisplayManagerSeat(const QString &name, QObject *parent)
        : QObject(parent), m_name(name), m_path(DISPLAYMANAGER_SEAT_PATH + name.mid(4)) {
        // create adaptor
        new SeatAdaptor(this);

        // register object
        QDBusConnection connection = (daemonApp->testing()) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService(DISPLAYMANAGER_SERVICE);
        connection.registerObject(m_path, this);
    }

    const QString &DisplayManagerSeat::Name() const {
        return m_name;
    }

    const QString &DisplayManagerSeat::Path() const {
        return m_path;
    }

    void DisplayManagerSeat::SwitchToGreeter() {
        daemonApp->seatManager()->switchToGreeter(m_name);
    }

    void DisplayManagerSeat::SwitchToGuest(const QString &/*session*/) {
        // TODO: IMPLEMENT
    }

    void DisplayManagerSeat::SwitchToUser(const QString &/*user*/, const QString &/*session*/) {
        // TODO: IMPLEMENT
    }

    void DisplayManagerSeat::Lock() {
        // TODO: IMPLEMENT
    }

    ObjectPathList DisplayManagerSeat::Sessions() {
       return daemonApp->displayManager()->Sessions(this);
    }

    DisplayManagerSession::DisplayManagerSession(const QString &name, const QString &seat, const QString &user, QObject *parent)
        : QObject(parent), m_name(name), m_path(DISPLAYMANAGER_SESSION_PATH + name.mid(7)), m_seat(seat), m_user(user) {
        // create adaptor
        new SessionAdaptor(this);

        // register object
        QDBusConnection connection = (daemonApp->testing()) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService(DISPLAYMANAGER_SERVICE);
        connection.registerObject(m_path, this);
    }

    const QString &DisplayManagerSession::Name() const {
        return m_name;
    }

    const QString &DisplayManagerSession::Path() const {
        return m_path;
    }

    const QString &DisplayManagerSession::Seat() const {
        return m_seat;
    }

    void DisplayManagerSession::Lock() {
        // TODO: IMPLEMENT
    }

    ObjectPath DisplayManagerSession::SeatPath() const {
        return ObjectPath(DISPLAYMANAGER_SEAT_PATH + m_seat.mid(4));
    }

    const QString &DisplayManagerSession::User() const {
        return m_user;
    }
}
