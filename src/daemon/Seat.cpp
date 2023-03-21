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
#include "XorgDisplayServer.h"
#include "VirtualTerminal.h"

#include <QDebug>
#include <QFile>
#include <QTimer>

#include <functional>
#include <optional>
#include <Login1Manager.h>
#include <Login1Session.h>

namespace SDDM {
    Seat::Seat(const QString &name, QObject *parent) : QObject(parent), m_name(name) {
        createDisplay(Display::defaultDisplayServerType());
    }

    const QString &Seat::name() const {
        return m_name;
    }

    void Seat::createDisplay(Display::DisplayServerType serverType) {
        //reload config if needed
        mainConfig.load();

        // create a new display
        qDebug() << "Adding new display...";
        Display *display = new Display(this, serverType);

        // restart display on stop
        connect(display, &Display::stopped, this, &Seat::displayStopped);
        connect(display, &Display::displayServerFailed, this, [this, display] {
            removeDisplay(display);

            // If we failed to create a display with wayland or rootful x11, try with
            // x11-user. There's a chance it might work. It's a handy fallback
            // since the alternative is a black screen
            if (display->displayServerType() != Display::X11UserDisplayServerType) {
                qWarning() << "Failed to launch the display server, falling back to DisplayServer=x11-user";
                createDisplay(Display::X11UserDisplayServerType);
            } else if (m_displays.isEmpty()) {
                qWarning() << "Failed to launch a DisplayServer=x11-user session, aborting";
                QCoreApplication::instance()->exit(12);
            }
        });

        // add display to the list
        m_displays << display;

        // start the display
        startDisplay(display);
    }

    void Seat::startDisplay(Display *display, int tryNr) {
        if (display->start())
            return;

        // It's possible that the system isn't ready yet (driver not loaded,
        // device not enumerated, ...). It's not possible to tell when that changes,
        // so try a few times with a delay in between.
        qWarning() << "Attempt" << tryNr << "starting the Display server on vt" << display->terminalId() << "failed";

        if(tryNr >= 3) {
            qCritical() << "Could not start Display server on vt" << display->terminalId();
            return;
        }

        QTimer::singleShot(2000, display, [=] { startDisplay(display, tryNr + 1); });
    }

    void Seat::removeDisplay(Display* display) {
        qDebug() << "Removing display" << display << "...";


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

        // If the display ended up reusing a session, it already jumped to it.
        if (!display->reuseSessionId().isEmpty())
            return;

        // If there is still a session running on some display,
        // switch to last display in display vector.
        if (!m_displays.isEmpty() && m_displays.constLast()->terminalId() > 0) {
            // Set vt_auto to true, so let the kernel handle the
            // vt switch automatically (VT_AUTO).
            VirtualTerminal::jumpToVt(m_displays.constLast()->terminalId(), true);
        } else {
            // restart otherwise
            createDisplay(Display::defaultDisplayServerType());
        }
    }
}
