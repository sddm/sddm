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

#include "XorgDisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "Seat.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QUuid>

#include <random>

#include <xcb/xcb.h>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    XorgDisplayServer::XorgDisplayServer(Display *parent) : DisplayServer(parent) {
        if (daemonApp->testing())
            m_xauth.setAuthDirectory(QStringLiteral("."));
        m_xauth.setup();
    }

    XorgDisplayServer::~XorgDisplayServer() {
        stop();
    }

    const QString &XorgDisplayServer::display() const {
        return m_display;
    }

    QString XorgDisplayServer::authPath() const {
        return m_xauth.authPath();
    }

    QString XorgDisplayServer::sessionType() const {
        return QStringLiteral("x11");
    }

    QString XorgDisplayServer::cookie() const {
        return m_xauth.cookie();
    }

    bool XorgDisplayServer::start() {
        // check flag
        if (m_started)
            return false;

        if (process) {
            qCritical() << "Tried to start Xorg before previous instance exited";
            return false;
        }

        // create process
        process = new QProcess(this);

        // delete process on finish
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &XorgDisplayServer::finished);

        // log message
        qDebug() << "Display server starting...";

        // generate auth file.
        // For the X server's copy, the display number doesn't matter.
        // An empty file would result in no access control!
        m_display = QStringLiteral(":0");
        if(!m_xauth.addCookie(m_display)) {
            qCritical() << "Failed to write xauth file";
            return false;
        }

        // set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
        QString xcursorSize = mainConfig.Theme.CursorSize.get();
        if (!xcursorSize.isEmpty())
            env.insert(QStringLiteral("XCURSOR_SIZE"), xcursorSize);
        process->setProcessEnvironment(env);

        //create pipe for communicating with X server
        //0 == read from X, 1== write to from X
        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            qCritical("Could not create pipe to start X server");
        }

        // start display server
        QStringList args;
        if (!daemonApp->testing()) {
            process->setProgram(mainConfig.X11.ServerPath.get());
            args << mainConfig.X11.ServerArguments.get().split(QLatin1Char(' '), Qt::SkipEmptyParts)
                 << QStringLiteral("-background") << QStringLiteral("none")
                 << QStringLiteral("-seat") << displayPtr()->seat()->name()
                 << QStringLiteral("vt%1").arg(displayPtr()->terminalId());
        } else {
            process->setProgram(mainConfig.X11.XephyrPath.get());
            args << QStringLiteral("-br")
                 << QStringLiteral("-screen") << QStringLiteral("800x600");
        }

        args << QStringLiteral("-auth") << m_xauth.authPath()
             << QStringLiteral("-noreset")
             << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);

        process->setArguments(args);
        qDebug() << "Running:"
            << qPrintable(process->program())
            << qPrintable(process->arguments().join(QLatin1Char(' ')));
        process->start();

        // wait for display server to start
        if (!process->waitForStarted()) {
            // log message
            qCritical() << "Failed to start display server process.";

            // return fail
            close(pipeFds[0]);
            return false;
        }

        // close the other side of pipe in our process, otherwise reading
        // from it may stuck even X server exit.
        close(pipeFds[1]);

        QFile readPipe;

        if (!readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
            qCritical("Failed to open pipe to start X Server");

            close(pipeFds[0]);
            stop();
            return false;
        }
        QByteArray displayNumber = readPipe.readLine();
        if (displayNumber.size() < 2) {
            // X server gave nothing (or a whitespace).
            qCritical("Failed to read display number from pipe");

            close(pipeFds[0]);
            stop();
            return false;
        }
        displayNumber.prepend(QByteArray(":"));
        displayNumber.remove(displayNumber.size() -1, 1); // trim trailing whitespace
        m_display = QString::fromLocal8Bit(displayNumber);

        // close our pipe
        close(pipeFds[0]);

        // The file is also used by the greeter, which does care about the
        // display number. Write the proper entry, if it's different.
        if(m_display != QStringLiteral(":0")) {
            if(!m_xauth.addCookie(m_display)) {
                qCritical() << "Failed to write xauth file";
                stop();
                return false;
            }
        }
        changeOwner(m_xauth.authPath());

        emit started();

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void XorgDisplayServer::stop() {
        if (!process)
            return;

        // log message
        qDebug() << "Display server stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
    }

    void XorgDisplayServer::finished() {
        // clean up
        if (process) {
            process->deleteLater();
            process = nullptr;
        }

        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << "Display server stopped.";

        QStringList displayStopCommand = QProcess::splitCommand(mainConfig.X11.DisplayStopCommand.get());

        // create display setup script process
        QProcess *displayStopScript = new QProcess();

        // set process environment
        QProcessEnvironment env;
        env.insert(QStringLiteral("DISPLAY"), m_display);
        env.insert(QStringLiteral("HOME"), QStringLiteral("/"));
        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
        displayStopScript->setProcessEnvironment(env);

        // start display stop script
        qDebug() << "Running display stop script " << displayStopCommand;
        const auto program = displayStopCommand.takeFirst();
        displayStopScript->start(program, displayStopCommand);

        // wait for finished
        if (!displayStopScript->waitForFinished(5000))
            displayStopScript->kill();

        // clean up the script process
        displayStopScript->deleteLater();
        displayStopScript = nullptr;

        // remove authority file
        QFile::remove(m_xauth.authPath());

        // emit signal
        emit stopped();
    }

    void XorgDisplayServer::setupDisplay() {
        // create cursor setup process
        QProcess *setCursor = new QProcess();
        // create display setup script process
        QProcess *displayScript = new QProcess();

        // set process environment
        QProcessEnvironment env;
        env.insert(QStringLiteral("DISPLAY"), m_display);
        env.insert(QStringLiteral("HOME"), QStringLiteral("/"));
        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        env.insert(QStringLiteral("XAUTHORITY"), m_xauth.authPath());
        env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
        env.insert(QStringLiteral("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
        setCursor->setProcessEnvironment(env);
        displayScript->setProcessEnvironment(env);

        qDebug() << "Setting default cursor";
        setCursor->start(QStringLiteral("xsetroot"), { QStringLiteral("-cursor_name"), QStringLiteral("left_ptr") });

        // delete setCursor on finish
        connect(setCursor, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), setCursor, &QProcess::deleteLater);

        // wait for finished
        if (!setCursor->waitForFinished(1000)) {
            qWarning() << "Could not setup default cursor";
            setCursor->kill();
        }

        // start display setup script
        qDebug() << "Running display setup script " << mainConfig.X11.DisplayCommand.get();
        QStringList displayCommand = QProcess::splitCommand(mainConfig.X11.DisplayCommand.get());
        const QString program = displayCommand.takeFirst();
        displayScript->start(program, displayCommand);

        // delete displayScript on finish
        connect(displayScript, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), displayScript, &QProcess::deleteLater);

        // wait for finished
        if (!displayScript->waitForFinished(30000))
            displayScript->kill();

        // reload config if needed
        mainConfig.load();
    }

    void XorgDisplayServer::changeOwner(const QString &fileName) {
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
