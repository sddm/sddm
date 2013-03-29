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

#include <QDebug>
#include <QFile>

#include <functional>

namespace SDDM {
    int findUnused(int minimum, std::function<bool(const int)> used) {
        // initialize with minimum
        int number = minimum;

        // find unused
        while (used(number))
            number++;

        // return number;
        return number;
    }

    Seat::Seat(const QString &name, QObject *parent) : QObject(parent) {
        // create seat adapter
        new SeatAdaptor(this);

        // set name
        m_name = QLatin1String("seat") + name;

        // set object path
        m_path = QLatin1String("/org/freedesktop/DisplayManager/Seat") + name;

        // register object
        QDBusConnection connection = (Configuration::instance()->testing) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService(QLatin1String("org.freedesktop.DisplayManager"));
        connection.registerObject(m_path, this);

        // create a display
        createDisplay();
    }

    const QString &Seat::name() const {
        return m_name;
    }

    const QString &Seat::path() const {
        return m_path;
    }

    void Seat::createDisplay(int displayId, int terminalId) {
        if (displayId == -1) {
            // find unused display
            displayId = findUnused(0, [&](const int number) {
                return m_displayIds.contains(number) || QFile(QString("/tmp/.X%1-lock").arg(number)).exists();
            });

            // find unused terminal
            terminalId = findUnused(Configuration::instance()->minimumVT, [&](const int number) {
                return m_terminalIds.contains(number);
            });
        }

        // mark display as used
        m_displayIds << displayId;

        // mark terminal as used
        m_terminalIds << terminalId;

        // log message
        qDebug() << " DAEMON: Adding new display :" << displayId << " on vt" << terminalId << "...";

        // create a new display
        Display *display = new Display(displayId, terminalId, this);

        // restart display on stop
        connect(display, SIGNAL(stopped()), this, SLOT(displayStopped()));

        // add display to the list
        m_displays << display;

        // start the display
        display->start();
    }

    void Seat::removeDisplay(int displayId) {
        qDebug() << " DAEMON: Removing display :" << displayId << "...";

        // display object
        Display *display = nullptr;

        // find display
        for (Display *d: m_displays)
            if (d->displayId() == displayId)
                display = d;

        // check if found
        if (display == nullptr)
            return;

        // remove display from list
        m_displays.removeAll(display);

        // mark display and terminal ids as unused
        m_displayIds.removeAll(display->displayId());
        m_terminalIds.removeAll(display->terminalId());

        // stop the display
        display->blockSignals(true);
        display->stop();
        display->blockSignals(false);

        // delete display
        display->deleteLater();
    }

    void Seat::displayStopped() {
        Display *display = qobject_cast<Display *>(sender());

        // remove display
        removeDisplay(display->displayId());

        // add a display if there is none
        if (m_displays.isEmpty())
            createDisplay();
    }

    bool Seat::CanSwitch() {
        return true;
    }

    bool Seat::HasGuestAccount() {
        return false;
    }

    QList<QDBusObjectPath> Seat::Sessions() {
        QList<QDBusObjectPath> sessions;

        for (Display *display: m_displays) {
            QString sessionPath = display->sessionPath();

            if (sessionPath != "")
                sessions << QDBusObjectPath(sessionPath);
        }

        return sessions;
    }

    void Seat::Lock() {
        // TODO: IMPLEMENT
    }

    void Seat::SwitchToGreeter() {
        createDisplay();
    }

    void Seat::SwitchToGuest(const QString &/*session*/) {
        // TODO: IMPLEMENT
    }

    void Seat::SwitchToUser(const QString &/*user*/, const QString &/*session*/) {
        // TODO: Implement
    }
}
