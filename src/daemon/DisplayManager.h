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

#ifndef SDDM_DISPLAYMANAGER_H
#define SDDM_DISPLAYMANAGER_H

#include <QObject>

#include <QDBusObjectPath>
#include <QList>

namespace SDDM {
    class DisplayManagerSeat;
    class DisplayManagerSession;

    typedef QDBusObjectPath ObjectPath;
    typedef QList<QDBusObjectPath> ObjectPathList;

    /***************************************************************************
     * org.freedesktop.DisplayManager
     **************************************************************************/
    class DisplayManager : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(DisplayManager)
        Q_PROPERTY(QList<QDBusObjectPath> Seats READ Seats CONSTANT)
        Q_PROPERTY(QList<QDBusObjectPath> Sessions READ Sessions CONSTANT)
    public:
        DisplayManager(QObject *parent = 0);

        QString seatPath(const QString &seatName);
        QString sessionPath(const QString &sessionName);

        ObjectPathList Seats() const;
        ObjectPathList Sessions(DisplayManagerSeat *seat = nullptr) const;

    public slots:
        void AddSeat(const QString &name);
        void RemoveSeat(const QString &name);
        void AddSession(const QString &name, const QString &seat, const QString &user);
        void RemoveSession(const QString &name);

    signals:
        void SeatAdded(ObjectPath seat);
        void SeatRemoved(ObjectPath seat);
        void SessionAdded(ObjectPath session);
        void SessionRemoved(ObjectPath session);

    private:
        QList<DisplayManagerSeat *> m_seats;
        QList<DisplayManagerSession *> m_sessions;
    };

    /***************************************************************************
     * org.freedesktop.DisplayManager.Seat
     **************************************************************************/
    class DisplayManagerSeat: public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(DisplayManagerSeat)
        Q_PROPERTY(bool CanSwitch READ CanSwitch CONSTANT)
        Q_PROPERTY(bool HasGuestAccount READ HasGuestAccount CONSTANT)
        Q_PROPERTY(QList<QDBusObjectPath> Sessions READ Sessions CONSTANT)
    public:
        DisplayManagerSeat(const QString &name, QObject *parent = 0);

        const QString &Name() const;
        const QString &Path() const;

        void SwitchToGreeter();
        void SwitchToGuest(const QString &session);
        void SwitchToUser(const QString &user, const QString &session);
        void Lock();

        bool CanSwitch() { return true; }
        bool HasGuestAccount() { return false; }
        ObjectPathList Sessions();

    private:
        QString m_name;
        QString m_path;
    };

    /***************************************************************************
     * org.freedesktop.DisplayManager.Session
     **************************************************************************/
    class DisplayManagerSession: public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(DisplayManagerSession)
        Q_PROPERTY(QDBusObjectPath Seat READ SeatPath)
        Q_PROPERTY(QString UserName READ User)
    public:
        DisplayManagerSession(const QString &name, const QString &seat, const QString &user, QObject *parent = 0);

        const QString &Name() const;
        const QString &Path() const;
        const QString &Seat() const;
        const QString &User() const;

        void Lock();

        ObjectPath SeatPath() const;

    private:
        QString m_name;
        QString m_path;
        QString m_seat;
        QString m_user;
    };
}

#endif // SDDM_DISPLAYMANAGER_H
