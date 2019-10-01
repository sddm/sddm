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

#ifndef SDDM_SEATMANAGER_H
#define SDDM_SEATMANAGER_H

#include <QObject>
#include <QHash>
#include <QDBusObjectPath>

namespace SDDM {
    class Seat;
    class LogindSeat;

    class SeatManager : public QObject {
        Q_OBJECT
    public:
        explicit SeatManager(QObject *parent = 0) : QObject(parent) {}

        void initialize();
        void createSeat(const QString &name);
        void removeSeat(const QString &name);
        void switchToGreeter(const QString &seat);

    Q_SIGNALS:
        void seatCreated(const QString &name);
        void seatRemoved(const QString &name);

    private Q_SLOTS:
        void logindSeatAdded(const QString &name, const QDBusObjectPath &objectPath);
        void logindSeatRemoved(const QString &name, const QDBusObjectPath &objectPath);

    private:
        QHash<QString, Seat *> m_seats; //these will exist only for graphical seats
        QHash<QString, LogindSeat*> m_systemSeats; //these will exist for all seats
    };
}

#endif // SDDM_SEATMANAGER_H
