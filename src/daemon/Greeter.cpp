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
#include "ThemeConfig.h"
#include "ThemeMetadata.h"
#include "Display.h"
#include "XorgUserDisplayServer.h"
#include "WaylandDisplayServer.h"

#include <QtCore/QDebug>
#include <QtCore/QProcess>

namespace SDDM {
    Greeter::Greeter(Display *parent)
        : QObject(parent)
        , m_display(parent)
    {
        m_metadata = new ThemeMetadata(QString());
        m_themeConfig = new ThemeConfig(QString());
    }

    Greeter::~Greeter() {
        stop();

        delete m_metadata;
        delete m_themeConfig;
    }

    void Greeter::setAuthPath(const QString &authPath) {
        m_authPath = authPath;
    }

    void Greeter::setSocket(const QString &socket) {
        m_socket = socket;
    }

    void Greeter::setTheme(const QString &theme) {
        m_themePath = theme;

        if (theme.isEmpty()) {
            m_metadata->setTo(QString());
            m_themeConfig->setTo(QString());
        } else {
            const QString path = QStringLiteral("%1/metadata.desktop").arg(m_themePath);
            m_metadata->setTo(path);

            QString configFile = QStringLiteral("%1/%2").arg(m_themePath).arg(m_metadata->configFile());
            m_themeConfig->setTo(configFile);
        }
    }

    QString Greeter::displayServerCommand() const
    {
        return m_displayServerCmd;
    }

    void Greeter::setDisplayServerCommand(const QString &cmd)
    {
        m_displayServerCmd = cmd;
    }

    bool Greeter::start() {
        // check flag
        if (m_started)
            return false;

        // themes
        QString xcursorTheme = mainConfig.Theme.CursorTheme.get();
        if (m_themeConfig->contains(QLatin1String("cursorTheme")))
            xcursorTheme = m_themeConfig->value(QLatin1String("cursorTheme")).toString();
        QString xcursorSize = mainConfig.Theme.CursorSize.get();
        if (m_themeConfig->contains(QLatin1String("cursorSize")))
            xcursorSize = m_themeConfig->value(QLatin1String("cursorSize")).toString();
        QString platformTheme;
        if (m_themeConfig->contains(QLatin1String("platformTheme")))
            platformTheme = m_themeConfig->value(QLatin1String("platformTheme")).toString();
        QString style;
        if (m_themeConfig->contains(QLatin1String("style")))
            style = m_themeConfig->value(QLatin1String("style")).toString();

        // greeter command
        QStringList args;
        args << QLatin1String("--socket") << m_socket;

        if (!m_themePath.isEmpty())
             args << QLatin1String("--theme") << m_themePath;
        if (!platformTheme.isEmpty())
            args << QLatin1String("-platformtheme") << platformTheme;
        if (!style.isEmpty())
            args << QLatin1String("-style") << style;

        Q_ASSERT(m_display);
        if (daemonApp->testing()) {
            // create process
            m_process = new QProcess(this);

            // delete process on finish
            connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Greeter::finished);

            connect(m_process, &QProcess::readyReadStandardOutput, this, &Greeter::onReadyReadStandardOutput);
            connect(m_process, &QProcess::readyReadStandardError, this, &Greeter::onReadyReadStandardError);

            // log message
            qDebug() << "Greeter starting...";

            args << QStringLiteral("--test-mode");

            if (m_display->displayServerType() == Display::X11DisplayServerType) {
                // set process environment
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert(QStringLiteral("DISPLAY"), m_display->name());
                env.insert(QStringLiteral("XAUTHORITY"), m_authPath);
                env.insert(QStringLiteral("XCURSOR_THEME"), xcursorTheme);
                if (!xcursorSize.isEmpty())
                    env.insert(QStringLiteral("XCURSOR_SIZE"), xcursorSize);
                m_process->setProcessEnvironment(env);
            }
            // Greeter command
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
            connect(m_auth, &Auth::requestChanged, this, &Greeter::onRequestChanged);
            connect(m_auth, &Auth::sessionStarted, this, &Greeter::onSessionStarted);
            connect(m_auth, &Auth::displayServerReady, this, &Greeter::onDisplayServerReady);
            connect(m_auth, &Auth::finished, this, &Greeter::onHelperFinished);
            connect(m_auth, &Auth::info, this, &Greeter::authInfo);
            connect(m_auth, &Auth::error, this, &Greeter::authError);

            // command
            QStringList cmd;
            cmd << QStringLiteral("%1/sddm-greeter").arg(QStringLiteral(BIN_INSTALL_DIR))
                << args;

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
            env.insert(QStringLiteral("XCURSOR_THEME"), xcursorTheme);
            if (!xcursorSize.isEmpty())
                env.insert(QStringLiteral("XCURSOR_SIZE"), xcursorSize);
            env.insert(QStringLiteral("XDG_SEAT"), m_display->seat()->name());
            env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(m_display->seat()->name()));
            env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
            if (m_display->seat()->name() == QLatin1String("seat0"))
                env.insert(QStringLiteral("XDG_VTNR"), QString::number(m_display->terminalId()));
            env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("greeter"));
            env.insert(QStringLiteral("XDG_SESSION_TYPE"), m_display->sessionType());
            if (m_display->displayServerType() == Display::X11DisplayServerType) {
                env.insert(QStringLiteral("DISPLAY"), m_display->name());
                env.insert(QStringLiteral("XAUTHORITY"), m_authPath);
                env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
            } else if (m_display->displayServerType() == Display::WaylandDisplayServerType) {
                env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
                env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
                env.insert(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"), QStringLiteral("fullscreen-shell-v1"));
            }
            m_auth->insertEnvironment(env);

            // log message
            qDebug() << "Greeter starting...";

            // start greeter
            m_auth->setUser(QStringLiteral("sddm"));
            m_auth->setDisplayServerCommand(m_displayServerCmd);
            m_auth->setGreeter(true);
            m_auth->setSession(cmd.join(QLatin1Char(' ')));
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
        } else {
            m_auth->stop();
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
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
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

    void Greeter::onDisplayServerReady(const QString &displayName)
    {
        auto *displayServer = m_display->displayServer();

        auto *xorgUser = qobject_cast<XorgUserDisplayServer *>(displayServer);
        if (xorgUser)
            xorgUser->setDisplayName(displayName);

        auto *wayland = qobject_cast<WaylandDisplayServer *>(displayServer);
        if (wayland)
            wayland->setDisplayName(displayName);
    }

    void Greeter::onHelperFinished(Auth::HelperExitStatus status) {
        // reset flag
        m_started = false;

        // log message
        qDebug() << "Greeter stopped.";

        // clean up
        m_auth->deleteLater();
        m_auth = nullptr;

        if (status == Auth::HELPER_SESSION_ERROR) {
            Q_EMIT failed();
        }
    }

    bool Greeter::isRunning() const {
        return (m_process && m_process->state() == QProcess::Running)
            || (m_auth->isActive());
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
