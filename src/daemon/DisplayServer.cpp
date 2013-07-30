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

#include "DisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"

#include <QDebug>
#include <QProcess>
#include <QTimer>

#include <xcb/xcb.h>

#include <unistd.h>

namespace SDDM {
    DisplayServer::DisplayServer(QObject *parent) : QObject(parent) {
    }

    DisplayServer::~DisplayServer() {
        stop();
    }

    void DisplayServer::setDisplay(const QString &display) {
        m_display = display;
    }

    void DisplayServer::setAuthPath(const QString &authPath) {
        m_authPath = authPath;
    }

    bool DisplayServer::start() {
        // check flag
        if (m_started)
            return false;

        // create process
        process = new QProcess(this);

        // delete process on finish
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        // log message
        qDebug() << " DAEMON: Display server starting...";

        if (daemonApp->configuration()->testing) {
            process->start("/usr/bin/Xephyr", { m_display, "-ac", "-br", "-noreset", "-screen",  "800x600"});
        } else {
            // set process environment
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("DISPLAY", m_display);
            env.insert("XAUTHORITY", m_authPath);
            env.insert("XCURSOR_THEME", daemonApp->configuration()->cursorTheme());
            process->setProcessEnvironment(env);

            // get display
            Display *display = qobject_cast<Display *>(parent());

            // start display server
            process->start(daemonApp->configuration()->serverPath(), { m_display, "-auth", m_authPath, "-nolisten", "tcp", QString("vt%1").arg(QString::number(display->terminalId()), 2, '0')});
        }

        // when the process starts, start trying to connect to it
        connect(process, SIGNAL(started()), SLOT(checkServerConnection()));

        // wait for display server to start
        if (!process->waitForStarted()) {
            // log message
            qCritical() << " DAEMON: Failed to start display server process.";

            // return fail
            return false;
        }

        // return success
        return true;
    }

    void DisplayServer::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << " DAEMON: Display server stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
    }

    void DisplayServer::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " DAEMON: Display server stopped.";

        // clean up
        process->deleteLater();
        process = nullptr;

        // emit signal
        emit stopped();
    }

    void DisplayServer::checkServerConnection(int msecsRemaining) {
        // get cookie from the display
        static QString cookie = qobject_cast<Display *>(parent())->cookie();

        // auth object
        static xcb_auth_info_t auth_info = { 18, strdup("MIT-MAGIC-COOKIE-1"), cookie.length(), strdup(qPrintable(cookie)) };

        // timeout
        if (msecsRemaining < 10000) {
            // log message
            qCritical() << " DAEMON: Failed to connect to the display server.";

            // stop the display server
            process->terminate();

            // notify about stopping this display server
            emit stopped();
            return;
        }
        else {
            // connection object
            xcb_connection_t *connection = nullptr;

            // try to connect to the server
            connection = xcb_connect_to_display_with_auth_info(qPrintable(m_display), &auth_info, nullptr);

            // check connection
            if (connection != nullptr) {
                xcb_disconnect(connection);

                // log message
                qDebug() << " DAEMON: Display server started.";

                // set flag
                m_started = true;

                // notify about possibility to use the server
                emit started();
            }
            else {
                // sleep for 10000 miliseconds
                QTimer::singleShot(10000, this, SLOT(checkServerConnection(msecsRemaining - 10000)));
            }
        }

        // free resources
        free(auth_info.data);
        free(auth_info.name);
    }
}
