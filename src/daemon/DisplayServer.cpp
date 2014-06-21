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
#include "SignalHandler.h"

#include <QDebug>
#include <QProcess>

#include <xcb/xcb.h>

#include <unistd.h>

namespace SDDM {
    DisplayServer::DisplayServer(Display *parent) : QObject(parent), m_displayPtr(parent) {
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
        qDebug() << "Display server starting...";

        if (daemonApp->configuration()->testing) {
            process->start("/usr/bin/Xephyr", { m_display, "-ac", "-br", "-noreset", "-screen",  "800x600"});
        } else {
            // set process environment
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("DISPLAY", m_display);
            env.insert("XAUTHORITY", m_authPath);
            env.insert("XCURSOR_THEME", daemonApp->configuration()->cursorTheme());
            process->setProcessEnvironment(env);

            // tell the display server to notify us when we can connect
            SignalHandler::ignoreSigusr1();

            // start display server
            process->start(daemonApp->configuration()->serverPath(), { m_display, "-auth", m_authPath, "-nolisten", "tcp", QString("vt%1").arg(m_displayPtr->terminalId()) });
            SignalHandler::initializeSigusr1();
        }

        // wait for display server to start
        if (!process->waitForStarted()) {
            // log message
            qCritical() << "Failed to start display server process.";

            // return fail
            return false;
        }

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void DisplayServer::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << "Display server stopping...";

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
        qDebug() << "Display server stopped.";

        // clean up
        process->deleteLater();
        process = nullptr;

        // emit signal
        emit stopped();
    }

    void DisplayServer::setupDisplay() {
        QString displayCommand = daemonApp->configuration()->displayCommand();

        // create display setup script process
        QProcess *displayScript = new QProcess();

        // set process environment
        QProcessEnvironment env;
        env.insert("DISPLAY", m_display);
        env.insert("HOME", "/");
        env.insert("PATH", daemonApp->configuration()->defaultPath());
        env.insert("XAUTHORITY", m_authPath);
        env.insert("SHELL", "/bin/sh");
        displayScript->setProcessEnvironment(env);

        // start display setup script
        qDebug() << "Running display setup script " << displayCommand;
        displayScript->start(displayCommand);
    }
}
