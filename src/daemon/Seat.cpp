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

#include "Seat.h"

#include "Configuration.h"
#include "Display.h"

#include "seatadaptor.h"

namespace SDDM {
    Seat::Seat(const QString &name, QObject *parent) : QObject(parent), m_name(name) {
        // create seat adapter
        new SeatAdaptor(this);

        // set object path
        m_path = QLatin1String("/org/freedesktop/DisplayManager/") + m_name;

        // register object
        QDBusConnection connection = (Configuration::instance()->testing) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService("org.freedesktop.DisplayManager");
        connection.registerObject(m_path, this);

        // mark display ":0" and vt07 as used, in test mode
        if (Configuration::instance()->testing) {
            m_displayIds << 0;
            m_terminalIds << 7;
        }
    }

    const QString &Seat::name() const {
        return m_name;
    }

    const QString &Seat::path() const {
        return m_path;
    }

    void Seat::start() {
        addDisplay();
    }

    void Seat::stop() {
        // stop all displays
        for (Display *display: m_displays)
            display->stop();
    }

    void Seat::addDisplay() {
        // find unused display
        int displayId = findUnused(m_displayIds, 0);

        // find unused terminal
        int terminalId = findUnused(m_terminalIds, Configuration::instance()->minimumVT);

        // log message
        qDebug() << " DAEMON: Adding new display " << QString(":%1").arg(displayId) << " on vt" << terminalId << "...";

        // create a new display
        Display *display = new Display(displayId, terminalId, this);

        // add display to the list
        m_displays << display;

        // start the display
        display->start();
    }

    void Seat::removeDisplay() {
        Display *display = qobject_cast<Display *>(sender());

        // log message
        qDebug() << " DAEMON: Removing display" << display->name() << "...";

        // remove display from list
        m_displays.removeAll(display);

        // mark display and terminal ids as unused
        m_displayIds.removeAll(display->displayId());
        m_terminalIds.removeAll(display->terminalId());

        // delete display
        display->deleteLater();
    }

    int Seat::findUnused(QList<int> &used, int minimum) {
        // initialize with minimum
        int number = minimum;

        // find unused
        while (used.contains(number))
            number++;

        // mark number as used
        used << number;

        // return number;
        return number;
    }

    bool Seat::CanSwitch() {
        return true;
    }

    bool Seat::HasGuestAccount() {
        return false;
    }

    void Seat::Lock() {
        // TODO: Implement
    }

    void Seat::SwitchToGreeter() {
        addDisplay();
    }

    void Seat::SwitchToGuest(const QString &session) {
        // TODO: Implement
    }

    void Seat::SwitchToUser(const QString &user, const QString &session) {
        // TODO: Implement
    }
}
