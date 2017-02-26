/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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
#include "DaemonApp.h"
#include "Display.h"

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

    Seat::Seat(const QString &name, QObject *parent) : QObject(parent), m_name(name) {
        createDisplay();
    }

    const QString &Seat::name() const {
        return m_name;
    }

    void Seat::createDisplay() {
        //reload config if needed
        mainConfig.load();

        // log message
        qDebug() << "Adding new display for" << m_name;

        // create a new display
        Display *display = new Display(this);

        // restart display on stop
        connect(display, SIGNAL(stopped()), this, SLOT(displayStopped()));

        // add display to the list
        m_displays << display;

        // start the display
        display->start();
    }

    void Seat::removeDisplay(Display* display) {
        qDebug() << "Removing display for" << m_name;


        // remove display from list
        m_displays.removeAll(display);

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

        // restart otherwise
        if (m_displays.isEmpty())
            createDisplay();
    }
}
