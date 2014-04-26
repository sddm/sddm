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

#include "WaylandDisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "Utils.h"

#include <QDebug>
#include <QFile>
#include <QProcess>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <linux/kd.h>
#include <fcntl.h>
#include <unistd.h>
#include <wayland-client.h>

namespace SDDM {
    WaylandDisplayServer::WaylandDisplayServer(Display *parent) : DisplayServer(parent) {
        // figure out the Wayland socket name
        m_display = QString("sddm-%1").arg(displayPtr()->displayId());

        if (!daemonApp->configuration()->testing) {
            // open vt
            const QString ttyName = QString("/dev/tty%1").arg(displayPtr()->terminalId());
            m_ttyFd = open(qPrintable(ttyName), O_RDWR);
            if (m_ttyFd == -1) {
                qCritical("Unable to open vt %d: %s", displayPtr()->terminalId(),
                          strerror(errno));
            } else {
                // make sure vt was not left in graphics mode by another display server
                int oldMode;
                ioctl(m_ttyFd, KDGETMODE, &oldMode);

                if (oldMode == KD_GRAPHICS)
                    ioctl(m_ttyFd, KDSETMODE, KD_TEXT);

                // make this the controlling terminal
                if (ioctl(m_ttyFd, TIOCSCTTY, 1) == -1)
                    qCritical("Unable to make vt %d the controlling terminal: %s",
                              displayPtr()->terminalId(), strerror(errno));
            }
        }
    }

    WaylandDisplayServer::~WaylandDisplayServer() {
        stop();
    }

    bool WaylandDisplayServer::displayExists(int number) {
        QString runtimeDir = daemonApp->configuration()->runtimeDir();
        return QFile(QString("%1/sddm-%2.lock").arg(runtimeDir).arg(number)).exists();
    }

    QString WaylandDisplayServer::sessionType() const {
        return QStringLiteral("wayland");
    }

    bool WaylandDisplayServer::start() {
        // check flag
        if (m_started)
            return false;

        // create process
        process = new QProcess(this);

        // delete process on finish
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        // start Wayland compositor for test mode
        if (daemonApp->configuration()->testing) {
            // log message
            qDebug() << "Starting Wayland compositor...";

            // start display server for testing
            process->start("/usr/bin/weston", {
                "--shell=fullscreen-shell.so",
                QString("--socket=%1").arg(m_display)
            });

            // wait for display server to start
            if (!process->waitForStarted()) {
                // log message
                qCritical() << "Failed to start Wayland compositor";

                // return fail
                return false;
            }

            // wait until we can connect to the display server
            if (!waitForStarted()) {
                // log message
                qCritical() << "Failed to connect to the Wayland compositor";

                // return fail
                return false;
            }

            // log message
            qDebug() << "Wayland compositor started";
        }

        // set flag
        m_started = true;

        // emit signal
        emit started();

        // return success
        return true;
    }

    void WaylandDisplayServer::stop() {
        // check flag
        if (!m_started)
            return;

        if (daemonApp->configuration()->testing) {
            // log message
            qDebug() << "Stopping Wayland compositor...";

            // terminate process
            process->terminate();

            // wait for finished
            if (!process->waitForFinished(5000))
                process->kill();
        }
    }

    void WaylandDisplayServer::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        if (daemonApp->configuration()->testing) {
            // log message
            qDebug() << "Wayland compositor stopped";

            // clean up
            process->deleteLater();
            process = nullptr;
        }

        // close vt if needed
        if (m_ttyFd != -1)
            close(m_ttyFd);

        // emit signal
        emit stopped();
    }

    void WaylandDisplayServer::setupDisplay() {
    }

    bool WaylandDisplayServer::waitForStarted(int msecs) {
        bool result = false;

        struct ::wl_display *display = nullptr;

        // try to connect to the compositor
        int i = msecs / 100;
        do {

            // try to connect to the compositor
            display = wl_display_connect(qPrintable(m_display));

            // check display
            if (display != nullptr)
                break;

            // sleep for 100 miliseconds
            usleep(100000);
        } while (i--);

        if (display != nullptr) {
            // disconnect
            wl_display_disconnect(display);

            // set success flag
            result = true;
        }

        // return result
        return result;
    }
}
