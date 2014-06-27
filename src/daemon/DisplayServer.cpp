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

#include "DisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "SignalHandler.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>

#include <xcb/xcb.h>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    DisplayServer::DisplayServer(Display *parent) : QObject(parent), m_displayPtr(parent) {
        // figure out the X11 display
        m_display = QString(":%1").arg(m_displayPtr->displayId());

        // get state directory
        QString stateDir = daemonApp->configuration()->stateDir();

        // use "." as authdir in test mode
        if (daemonApp->configuration()->testing)
            stateDir = QLatin1String(".");

        // create auth dir if not existing
        QDir().mkpath(stateDir);

        // set auth path
        m_authPath = QString("%1/%2").arg(stateDir).arg(m_display);

        // generate cookie
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        // resever 32 bytes
        m_cookie.reserve(32);

        // create a random hexadecimal number
        const char *digits = "0123456789abcdef";
        for (int i = 0; i < 32; ++i)
            m_cookie[i] = digits[dis(gen)];
    }

    DisplayServer::~DisplayServer() {
        stop();
    }

    const QString &DisplayServer::display() const {
        return m_display;
    }

    const QString &DisplayServer::authPath() const {
        return m_authPath;
    }

    const QString &DisplayServer::cookie() const {
        return m_cookie;
    }

    void DisplayServer::addCookie(const QString &file) {
        // log message
        qDebug() << "Adding cookie to" << file;

        // Touch file
        QFile file_handler(file);
        file_handler.open(QIODevice::WriteOnly);
        file_handler.close();

        QString cmd = QString("%1 -f %2 -q").arg(daemonApp->configuration()->xauthPath()).arg(file);

        // execute xauth
        FILE *fp = popen(qPrintable(cmd), "w");

        // check file
        if (!fp)
            return;
        fprintf(fp, "remove %s\n", qPrintable(m_display));
        fprintf(fp, "add %s . %s\n", qPrintable(m_display), qPrintable(m_cookie));
        fprintf(fp, "exit\n");

        // close pipe
        pclose(fp);
    }

    bool DisplayServer::start() {
        // check flag
        if (m_started)
            return false;

        // generate auth file
        addCookie(m_authPath);
        changeOwner(m_authPath);

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
            QStringList args;
            args << m_display
                 << "-auth" << m_authPath
                 << "-nolisten" << "tcp"
                 << "-background" << "none"
                 << "-noreset"
                 << QString("vt%1").arg(m_displayPtr->terminalId());
            process->start(daemonApp->configuration()->serverPath(), args);
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

        // remove authority file
        QFile::remove(m_authPath);

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

    void DisplayServer::changeOwner(const QString &fileName) {
        // change the owner and group of the auth file to the sddm user
        struct passwd *pw = getpwnam("sddm");
        if (!pw)
            qWarning() << "Failed to find the sddm user. Owner of the auth file will not be changed.";
        else {
            if (chown(qPrintable(fileName), pw->pw_uid, pw->pw_gid) == -1)
                qWarning() << "Failed to change owner of the auth file.";
        }
    }
}
