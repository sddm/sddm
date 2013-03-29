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

#ifndef SDDM_SEAT_H
#define SDDM_SEAT_H

#include <QObject>

#include <QDBusObjectPath>
#include <QList>

namespace SDDM {
    class Display;

    class Seat : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Seat)
        Q_PROPERTY(bool CanSwitch READ CanSwitch CONSTANT)
        Q_PROPERTY(bool HasGuestAccount READ HasGuestAccount CONSTANT)
        Q_PROPERTY(QList<QDBusObjectPath> Sessions READ Sessions CONSTANT)
    public:
        explicit Seat(const QString &name, QObject *parent = 0);

        const QString &name() const;
        const QString &path() const;

    public slots:
        void createDisplay(int displayId = -1, int terminalId = -1);
        void removeDisplay(int displayId);

        bool CanSwitch();
        bool HasGuestAccount();
        QList<QDBusObjectPath> Sessions();

        void Lock();
        void SwitchToGreeter();
        void SwitchToGuest(const QString &session);
        void SwitchToUser(const QString &user, const QString &session);

    private slots:
        void displayStopped();

    private:
        QString m_name { "" };
        QString m_path { "" };

        QList<Display *> m_displays;
        QList<int> m_terminalIds;
        QList<int> m_displayIds;
    };
}

#endif // SDDM_SEAT_H
