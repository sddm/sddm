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
    Seat::Seat(const QString &name, QObject *parent) : QObject(parent) {
        // create seat adapter
        new SeatAdaptor(this);

        // set name
        m_name = QLatin1String("seat") + name;

        // set object path
        m_path = QLatin1String("/org/freedesktop/DisplayManager/Seat") + name;

        // register object
        QDBusConnection connection = (Configuration::instance()->testing) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService("org.freedesktop.DisplayManager");
        connection.registerObject(m_path, this);
    }

    Seat::~Seat() {
        stop();
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
        while (!m_displays.isEmpty())
            removeDisplay(m_displays.first());
    }

    void Seat::addDisplay() {
        // find unused display
        int displayId = findUnused(0, [&](const int number) {
            return m_displayIds.contains(number) || QFile(QString("/tmp/.X%1-lock").arg(number)).exists();
        });

        // mark display as used
        m_displayIds << displayId;

        // find unused terminal
        int terminalId = findUnused(Configuration::instance()->minimumVT, [&](const int number) {
            return m_terminalIds.contains(number);
        });

        // mark terminal as used
        m_terminalIds << terminalId;

        // log message
        qDebug() << " DAEMON: Adding new display " << QString(":%1").arg(displayId) << " on vt" << terminalId << "...";

        // create a new display
        Display *display = new Display(displayId, terminalId, this);

        // restart display on stop
        connect(display, SIGNAL(stopped()), this, SLOT(displayStopped()));

        // add display to the list
        m_displays << display;

        // start the display
        display->start();
    }

    void Seat::removeDisplay(Display *display) {
        // log message
        qDebug() << " DAEMON: Removing display" << display->name() << "...";

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
        removeDisplay(display);

        // add a display if there is none
        if (m_displays.isEmpty())
            addDisplay();
    }

    int Seat::findUnused(int minimum, std::function<bool(const int)> used) {
        // initialize with minimum
        int number = minimum;

        // find unused
        while (used(number))
            number++;

        // return number;
        return number;
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
