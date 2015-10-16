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

#include "SeatManager.h"

#include "Seat.h"

namespace SDDM {
    SeatManager::SeatManager(QObject *parent) : QObject(parent) {
    }

    void SeatManager::createSeat(const QString &name) {
        // create a seat
        Seat *seat = new Seat(name, this);

        // add to the list
        m_seats.insert(name, seat);

        // emit signal
        emit seatCreated(name);
    }

    void SeatManager::removeSeat(const QString &name) {
        // check if seat exists
        if (!m_seats.contains(name))
            return;

        // remove from the list
        Seat *seat = m_seats.take(name);

        // delete seat
        seat->deleteLater();

        // emit signal
        emit seatRemoved(name);
    }

    void SeatManager::switchToGreeter(const QString &name) {
        // check if seat exists
        if (!m_seats.contains(name))
            return;

        // switch to greeter
        m_seats.value(name)->createDisplay();
    }
}
