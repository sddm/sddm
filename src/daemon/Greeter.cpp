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
#include "DaemonApp.h"
#include "DisplayManager.h"
#include "Seat.h"
#include "Display.h"

#include <QtCore/QDebug>
#include <QtCore/QProcess>

namespace SDDM {
    Greeter::Greeter(QObject *parent) : QObject(parent) {
    }

    Greeter::~Greeter() {
        stop();
    }

    void Greeter::setDisplay(Display *display) {
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

        if (daemonApp->testing()) {
            // create process
            m_process = new QProcess(this);

            // delete process on finish
            connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

            connect(m_process, SIGNAL(readyReadStandardOutput()), SLOT(onReadyReadStandardOutput()));
            connect(m_process, SIGNAL(readyReadStandardError()), SLOT(onReadyReadStandardError()));

            // log message
            qDebug() << "Greeter starting...";

            // set process environment
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("DISPLAY"), m_display->name());
            env.insert(QStringLiteral("XAUTHORITY"), m_authPath);
            env.insert(QStringLiteral("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
            m_process->setProcessEnvironment(env);

            // start greeter
            QStringList args;
            if (daemonApp->testing())
                args << QStringLiteral("--test-mode");
            args << QStringLiteral("--socket") << m_socket
                 << QStringLiteral("--theme") << m_theme;
            m_process->start(QStringLiteral("%1/sddm-greeter").arg(QStringLiteral(BIN_INSTALL_DIR)), args);

            //if we fail to start bail immediately, and don't block in waitForStarted
            if (m_process->state() == QProcess::NotRunning) {
                qCritical() << "Greeter failed to launch.";
                return false;
            }
            // wait for greeter to start
            if (!m_process->waitForStarted()) {
                // log message
                qCritical() << "Failed to start greeter.";

                // return fail
                return false;
            }

            // log message
            qDebug() << "Greeter started.";

            // set flag
            m_started = true;
        } else {
            // authentication
            m_auth = new Auth(this);
            m_auth->setVerbose(true);
            connect(m_auth, SIGNAL(requestChanged()), this, SLOT(onRequestChanged()));
            connect(m_auth, SIGNAL(session(bool)), this, SLOT(onSessionStarted(bool)));
            connect(m_auth, SIGNAL(finished(Auth::HelperExitStatus)), this, SLOT(onHelperFinished(Auth::HelperExitStatus)));
            connect(m_auth, SIGNAL(info(QString,Auth::Info)), this, SLOT(authInfo(QString,Auth::Info)));
            connect(m_auth, SIGNAL(error(QString,Auth::Error)), this, SLOT(authError(QString,Auth::Error)));

            // greeter command
            QStringList args;
            args << QStringLiteral("%1/sddm-greeter").arg(QStringLiteral(BIN_INSTALL_DIR));
            args << QStringLiteral("--socket") << m_socket
                 << QStringLiteral("--theme") << m_theme;

            // greeter environment
            QProcessEnvironment env;
            QProcessEnvironment sysenv = QProcessEnvironment::systemEnvironment();

            insertEnvironmentList({QStringLiteral("LANG"), QStringLiteral("LANGUAGE"),
                                   QStringLiteral("LC_CTYPE"), QStringLiteral("LC_NUMERIC"), QStringLiteral("LC_TIME"), QStringLiteral("LC_COLLATE"),
                                   QStringLiteral("LC_MONETARY"), QStringLiteral("LC_MESSAGES"), QStringLiteral("LC_PAPER"), QStringLiteral("LC_NAME"),
                                   QStringLiteral("LC_ADDRESS"), QStringLiteral("LC_TELEPHONE"), QStringLiteral("LC_MEASUREMENT"), QStringLiteral("LC_IDENTIFICATION"),
                                   QStringLiteral("LD_LIBRARY_PATH"),
                                   QStringLiteral("QML2_IMPORT_PATH"),
                                   QStringLiteral("QT_PLUGIN_PATH"),
                                   QStringLiteral("XDG_DATA_DIRS")
            }, sysenv, env);

            env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
            env.insert(QStringLiteral("DISPLAY"), m_display->name());
            env.insert(QStringLiteral("XAUTHORITY"), m_authPath);
            env.insert(QStringLiteral("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
            env.insert(QStringLiteral("XDG_SEAT"), m_display->seat()->name());
            env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(m_display->seat()->name()));
            env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
            env.insert(QStringLiteral("XDG_VTNR"), QString::number(m_display->terminalId()));
            env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("greeter"));
            env.insert(QStringLiteral("XDG_SESSION_TYPE"), m_display->sessionType());

            //some themes may use KDE components and that will automatically load KDE's crash handler which we don't want
            //counterintuitively setting this env disables that handler
            env.insert(QStringLiteral("KDE_DEBUG"), QStringLiteral("1"));
            m_auth->insertEnvironment(env);

            // log message
            qDebug() << "Greeter starting...";

            // start greeter
            m_auth->setUser(QStringLiteral("sddm"));
            m_auth->setGreeter(true);
            m_auth->setSession(args.join(QLatin1Char(' ')));
            m_auth->start();
        }

        // return success
        return true;
    }

    void Greeter::insertEnvironmentList(QStringList names, QProcessEnvironment sourceEnv, QProcessEnvironment &targetEnv) {
        for (QStringList::const_iterator it = names.constBegin(); it != names.constEnd(); ++it)
            if (sourceEnv.contains(*it))
                targetEnv.insert(*it, sourceEnv.value(*it));
    }

    void Greeter::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << "Greeter stopping...";

        if (daemonApp->testing()) {
            // terminate process
            m_process->terminate();

            // wait for finished
            if (!m_process->waitForFinished(5000))
                m_process->kill();
        }
    }

    void Greeter::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << "Greeter stopped.";

        // clean up
        m_process->deleteLater();
        m_process = nullptr;
    }

    void Greeter::onRequestChanged() {
        m_auth->request()->setFinishAutomatically(true);
    }

    void Greeter::onSessionStarted(bool success) {
        // set flag
        m_started = success;

        // log message
        if (success)
            qDebug() << "Greeter session started successfully";
        else
            qDebug() << "Greeter session failed to start";
    }

    void Greeter::onHelperFinished(Auth::HelperExitStatus status) {
        // reset flag
        m_started = false;

        // log message
        qDebug() << "Greeter stopped.";

        // clean up
        m_auth->deleteLater();
        m_auth = nullptr;
    }

    void Greeter::onReadyReadStandardError()
    {
        if (m_process) {
            qDebug() << "Greeter errors:" << qPrintable(QString::fromLocal8Bit(m_process->readAllStandardError()));
        }
    }

    void Greeter::onReadyReadStandardOutput()
    {
        if (m_process) {
            qDebug() << "Greeter output:" << qPrintable(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
        }
    }

    void Greeter::authInfo(const QString &message, Auth::Info info) {
        Q_UNUSED(info);
        qDebug() << "Information from greeter session:" << message;
    }

    void Greeter::authError(const QString &message, Auth::Error error) {
        Q_UNUSED(error);
        qWarning() << "Error from greeter session:" << message;
    }
}
