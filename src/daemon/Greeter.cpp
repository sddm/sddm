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

#include "Greeter.h"

#include "Configuration.h"
#include "Constants.h"

#include <QDebug>
#include <QProcess>

namespace SDDM {
    Greeter::Greeter(QObject *parent) : QObject(parent) {
    }

    Greeter::~Greeter() {
        stop();
    }

    void Greeter::setDisplay(const QString &display) {
        m_display = display;
    }

    void Greeter::setAuthPath(const QString &authPath) {
        m_authPath = authPath;
    }

    void Greeter::setSocket(const QString &socket) {
        m_socket = socket;
    }

    void Greeter::setTheme(const QString &theme) {
        m_theme = theme;
    }

    bool Greeter::start() {
        // check flag
        if (m_started)
            return false;

        // create process
        process = new QProcess(this);

        // delete process on finish
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        // log message
        qDebug() << " DAEMON: Greeter starting...";

        // set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("DISPLAY", m_display);
        env.insert("XAUTHORITY", m_authPath);
        env.insert("XCURSOR_THEME", Configuration::instance()->cursorTheme());
        process->setProcessEnvironment(env);

#ifndef TEST
        alsdjkfal;sdjf
        // start greeter
        process->start(QString("%1/sddm-greeter").arg(BIN_INSTALL_DIR), { "--socket", m_socket, "--theme", m_theme });
#else
        // start greeter
        process->start(QString("%1/sddm-greeter").arg("."), { "--socket", m_socket, "--theme", m_theme });
#endif
        // wait for greeter to start
        if (!process->waitForStarted()) {
            // log message
            qCritical() << " DAEMON: Failed to start greeter.";

            // return fail
            return false;
        }

        // log message
        qDebug() << " DAEMON: Greeter started.";

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void Greeter::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << " DAEMON: Greeter stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
    }

    void Greeter::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " DAEMON: Greeter stopped.";

        // clean up
        process->deleteLater();
        process = nullptr;
    }
}
