/*
 * This file is part of SDDM.
 *
 * Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <QtCore/QDir>
#include <QtCore/QUuid>

#include "Configuration.h"

#include "launcher.h"
#include "xorgserver.h"

#include <random>

#include <pwd.h>
#include <unistd.h>

XorgServer::XorgServer(Launcher *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_started(false)
{
    // Run display stop script when the server shuts down
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));
}

bool XorgServer::isRunning() const
{
    return m_started;
}

QString XorgServer::display() const
{
    return m_display;
}

QString XorgServer::authPath() const
{
    return m_authPath;
}

bool XorgServer::start()
{
    Launcher *launcher = qobject_cast<Launcher *>(parent());

    // Display
    if (launcher->isTestModeEnabled())
        m_display = QLatin1String(":2048");

    // Auth directory ("." in test mode)
    QString authDir = launcher->isTestModeEnabled()
            ? QLatin1String(".") : QLatin1String(RUNTIME_DIR);
    QDir().mkpath(authDir);

    // Auth path
    m_authPath = QStringLiteral("%1/%2").arg(authDir).arg(QUuid::createUuid().toString());

    // Cookie
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    m_cookie.reserve(32);
    const char *digits = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
        m_cookie[i] = digits[dis(gen)];

    if (launcher->isTestModeEnabled()) {
        // Run Xephyr
        QStringList args;
        args << m_display
             << QLatin1String("-ac")
             << QLatin1String("-br")
             << QLatin1String("-noreset")
             << QLatin1String("-screen") << QLatin1String("800x600");
        m_process->start(SDDM::mainConfig.X11.XephyrPath.get(), args);
        qDebug() << "Running:"
                 << qPrintable(SDDM::mainConfig.X11.XephyrPath.get())
                 << qPrintable(args.join(QLatin1Char(' ')));

        if (!m_process->waitForStarted()) {
            qCritical("Failed to start Xorg server");
            return false;
        }

        qInfo("Xorg server started");
        emit started();
        m_started = true;

        // Generate auth file
        addCookie(m_authPath);
    } else {
        // Process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QLatin1String("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
        env.insert(QLatin1String("XCURSOR_THEME"), SDDM::mainConfig.Theme.CursorTheme.get());
        if (SDDM::mainConfig.X11.Unprivileged.get())
            env.insert(QLatin1String("XORG_RUN_AS_USER_OK"), QString::number(1));
        m_process->setProcessEnvironment(env);

        // Create pipe for communicating with X server
        // 0 == read from X, 1 == write to from X
        int pipeFds[2];
        if (::pipe(pipeFds) != 0) {
            qCritical("Could not create pipe to start X server");
        }

        // Run Xorg
        QStringList args = SDDM::mainConfig.X11.ServerArguments.get().split(QLatin1Char(' '), QString::SkipEmptyParts);
        args << QLatin1String("-auth") << m_authPath
             << QLatin1String("-background") << QLatin1String("none")
             << QLatin1String("-noreset")
             << QLatin1String("-displayfd") << QString::number(pipeFds[1])
             << QStringLiteral("vt%1").arg(env.value(QLatin1String("XDG_VTNR")));
        if (SDDM::mainConfig.X11.Unprivileged.get())
            args << QLatin1String("-keeptty");
        qDebug() << "Running:"
                 << qPrintable(SDDM::mainConfig.X11.ServerPath.get())
                 << qPrintable(args.join(QLatin1Char(' ')));
        m_process->start(SDDM::mainConfig.X11.ServerPath.get(), args);

        if (!m_process->waitForStarted()) {
            qCritical("Failed to start Xorg server");
            ::close(pipeFds[0]);
            ::close(pipeFds[1]);
            return false;
        }

        // Close the other side of pipe in our process, otherwise reading
        // from it may stuck even X server exit
        ::close(pipeFds[1]);

        // Read the display name from the pipe
        QFile readPipe;
        if (!readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
            qCritical("Failed to open pipe to start Xorg server");
            ::close(pipeFds[0]);
            return false;
        }
        QByteArray displayNumber = readPipe.readLine().trimmed();
        if (displayNumber.isEmpty()) {
            qCritical("No Xorg display name returned from server, something is wrong");
            ::close(pipeFds[0]);
            return false;
        }
        displayNumber.prepend(QByteArray(":"));
        m_display = QString::fromLocal8Bit(displayNumber);
        qDebug() << "Xorg display name:" << m_display;

        // Close our pipe
        ::close(pipeFds[0]);

        // Generate auth file
        addCookie(m_authPath);
        if (!SDDM::mainConfig.X11.Unprivileged.get() && launcher->session().isEmpty())
            changeOwner(m_authPath);

        qInfo("Xorg server started");
        emit started();
        m_started = true;

        // Setup display
        setupDisplay();
    }

    return true;
}

void XorgServer::stop()
{
    m_process->terminate();
    if (!m_process->waitForFinished())
        m_process->kill();
}

void XorgServer::addCookie(const QString &file)
{
    qInfo() << "Adding cookie to" << file;

    // Touch file
    QFile file_handler(file);
    file_handler.open(QIODevice::WriteOnly);
    file_handler.close();

    QString cmd = QStringLiteral("%1 -f %2 -q").arg(SDDM::mainConfig.X11.XauthPath.get()).arg(file);

    // Execute xauth
    FILE *fp = popen(qPrintable(cmd), "w");
    if (!fp)
        return;
    fprintf(fp, "remove %s\n", qPrintable(m_display));
    fprintf(fp, "add %s . %s\n", qPrintable(m_display), qPrintable(m_cookie));
    fprintf(fp, "exit\n");
    pclose(fp);
}

void XorgServer::changeOwner(const QString &fileName)
{
    // Change the owner and group of the auth file to the sddm user
    struct passwd *pw = getpwnam("sddm");
    if (!pw) {
        qWarning("Failed to find the sddm user. Owner of the auth file will not be changed.");
    } else {
        if (::chown(qPrintable(fileName), pw->pw_uid, pw->pw_gid) == -1)
            qWarning("Failed to change owner of the auth file");
    }
}

void XorgServer::setupDisplay()
{
    // Check flag
    if (!m_started)
        return;

    QString displayCommand = SDDM::mainConfig.X11.DisplayCommand.get();

    // Create display setup script process
    QProcess *displayScript = new QProcess();

    // Process environment
    QProcessEnvironment env;
    env.insert(QLatin1String("DISPLAY"), m_display);
    env.insert(QLatin1String("HOME"), QLatin1String("/"));
    env.insert(QLatin1String("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
    env.insert(QLatin1String("XAUTHORITY"), m_authPath);
    env.insert(QLatin1String("SHELL"), QLatin1String("/bin/sh"));
    displayScript->setProcessEnvironment(env);

    // Delete displayScript on finish
    connect(displayScript, SIGNAL(finished(int,QProcess::ExitStatus)), displayScript, SLOT(deleteLater()));

    // Start display setup script
    qDebug() << "Running display setup script" << displayCommand;
    displayScript->start(displayCommand);

    // Wait for finished
    if (!displayScript->waitForFinished(30000))
        displayScript->kill();

    // Reload config if needed
    SDDM::mainConfig.load();
}

void XorgServer::finished()
{
    // Check flag
    if (!m_started)
        return;

    // Reset flag
    m_started = false;

    // log message
    qInfo() << "Xorg server stopped";

    QString displayStopCommand = SDDM::mainConfig.X11.DisplayStopCommand.get();

    // Create display setup script process
    QProcess *displayStopScript = new QProcess();

    // Process environment
    QProcessEnvironment env;
    env.insert(QLatin1String("DISPLAY"), m_display);
    env.insert(QLatin1String("HOME"), QLatin1String("/"));
    env.insert(QLatin1String("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
    env.insert(QLatin1String("SHELL"), QLatin1String("/bin/sh"));
    displayStopScript->setProcessEnvironment(env);

    // Start display setup script
    qDebug() << "Running display stop script" << displayStopCommand;
    displayStopScript->start(displayStopCommand);

    // Wait for finished
    if (!displayStopScript->waitForFinished(5000))
        displayStopScript->kill();

    // Clean up the script process
    displayStopScript->deleteLater();
    displayStopScript = nullptr;

    // Remove authority file
    QFile::remove(m_authPath);

    // Emit signal
    emit stopped();
}
