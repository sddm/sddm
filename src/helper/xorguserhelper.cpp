/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>

#include "Configuration.h"

#include "xorguserhelper.h"

#include <fcntl.h>
#include <unistd.h>

namespace SDDM {

XOrgUserHelper::XOrgUserHelper(QObject *parent)
    : QObject(parent)
{
}

QProcessEnvironment XOrgUserHelper::sessionEnvironment() const
{
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_display);
    env.insert(QStringLiteral("XAUTHORITY"), m_xauth.authPath());
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
    return env;
}

QString XOrgUserHelper::display() const
{
    return m_display;
}

bool XOrgUserHelper::start(const QString &cmd)
{
    // Create xauthority
    m_xauth.setAuthDirectory(qEnvironmentVariable("XDG_RUNTIME_DIR"));
    m_xauth.setup();

    // Start server process
    if (!startServer(cmd))
        return false;

    // Setup display
    startDisplayCommand();

    return true;
}

void XOrgUserHelper::stop()
{
    if (m_serverProcess) {
        qInfo("Stopping server...");
        m_serverProcess->terminate();
        if (!m_serverProcess->waitForFinished(5000)) {
            m_serverProcess->kill();
            m_serverProcess->waitForFinished(25000);
        }
        m_serverProcess->deleteLater();
        m_serverProcess = nullptr;

        displayFinished();
    }
}

bool XOrgUserHelper::startProcess(const QString &cmd,
                                  const QProcessEnvironment &env,
                                  QProcess **p)
{
    auto args = QProcess::splitCommand(cmd);
    const auto program = args.takeFirst();

    // Make sure to forward the input of this process into the Xorg
    // server, otherwise it will complain that only console users are allowed
    auto *process = new QProcess(this);
    process->setProcessEnvironment(env);
    process->setInputChannelMode(QProcess::ForwardedInputChannel);
    connect(process, &QProcess::readyReadStandardError, this, [process] {
        qWarning() << process->readAllStandardError();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] {
        qInfo() << process->readAllStandardOutput();
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process, [](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode != 0 || exitStatus != QProcess::NormalExit)
            QCoreApplication::instance()->quit();
    });

    process->start(program, args);
    if (!process->waitForStarted(10000)) {
        qWarning("Failed to start \"%s\": %s",
                 qPrintable(cmd),
                 qPrintable(process->errorString()));
        return false;
    }

    if (p)
        *p = process;

    return true;
}

bool XOrgUserHelper::startServer(const QString &cmd)
{
    QString serverCmd = cmd;

    // Create pipe for communicating with X server
    // 0 == read from X, 1 == write to X
    int pipeFds[2];
    if (::pipe(pipeFds) != 0) {
        qCritical("Could not create pipe to start X server");
        return false;
    }

    // Do not leak the read endpoint to the X server process
    fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);

    // Server environment
    // Not setting XORG_RUN_AS_USER_OK=1 will make Xorg require root privileges
    // under Fedora and all distros that use their patch.
    // https://src.fedoraproject.org/rpms/xorg-x11-server/blob/rawhide/f/0001-Fedora-hack-Make-the-suid-root-wrapper-always-start-.patch
    // https://fedoraproject.org/wiki/Changes/XorgWithoutRootRights
    auto serverEnv = QProcessEnvironment::systemEnvironment();
    serverEnv.insert(QStringLiteral("XORG_RUN_AS_USER_OK"), QStringLiteral("1"));

    // Append xauth and display fd to the command
    auto args = QStringList()
            << QStringLiteral("-auth") << m_xauth.authPath()
            << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);

    // Append VT from environment
    args << QStringLiteral("vt%1").arg(serverEnv.value(QStringLiteral("XDG_VTNR")));

    // Command string
    serverCmd += QLatin1Char(' ') + args.join(QLatin1Char(' '));

    // Start the server process
    qInfo("Running server: %s", qPrintable(serverCmd));
    if (!startProcess(serverCmd, serverEnv, &m_serverProcess)) {
        ::close(pipeFds[0]);
        return false;
    }

    // Close the other side of pipe in our process, otherwise reading
    // from it may stuck even X server exit
    ::close(pipeFds[1]);

    // Read the display number from the pipe
    QFile readPipe;
    if (!readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
        qCritical("Failed to open pipe to start X Server");
        ::close(pipeFds[0]);
        return false;
    }
    QByteArray displayNumber = readPipe.readLine();
    if (displayNumber.size() < 2) {
        // X server gave nothing (or a whitespace)
        qCritical("Failed to read display number from pipe");
        ::close(pipeFds[0]);
        return false;
    }
    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() -1, 1); // trim trailing whitespace
    m_display = QString::fromLocal8Bit(displayNumber);
    qDebug("X11 display: %s", qPrintable(m_display));
    Q_EMIT displayChanged(m_display);

    // Generate xauthority file
    // For the X server's copy, the display number doesn't matter.
    // An empty file would result in no access control!
    if (!m_xauth.addCookie(m_display)) {
        qCritical("Failed to write xauth file");
        return false;
    }

    // Close our pipe
    ::close(pipeFds[0]);

    return true;
}

void XOrgUserHelper::startDisplayCommand()
{
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_display);
    env.insert(QStringLiteral("XAUTHORITY"), m_xauth.authPath());

    // Set cursor
    qInfo("Setting default cursor...");
    QProcess *setCursor = nullptr;
    if (startProcess(QStringLiteral("xsetroot -cursor_name left_ptr"), env, &setCursor)) {
        if (!setCursor->waitForFinished(1000)) {
            qWarning() << "Could not setup default cursor";
            setCursor->kill();
        }
        setCursor->deleteLater();
    }

    // Display setup script
    auto cmd = mainConfig.X11.DisplayCommand.get();
    qInfo("Running display setup script: %s", qPrintable(cmd));
    QProcess *displayScript = nullptr;
    if (startProcess(cmd, env, &displayScript)) {
        if (!displayScript->waitForFinished(30000))
            displayScript->kill();
        displayScript->deleteLater();
    }
}

void XOrgUserHelper::displayFinished()
{
    auto cmd = mainConfig.X11.DisplayStopCommand.get();
    qInfo("Running display stop script: %s", qPrintable(cmd));
    QProcess *displayStopScript = nullptr;
    if (startProcess(cmd, sessionEnvironment(), &displayStopScript)) {
        if (!displayStopScript->waitForFinished(5000))
            displayStopScript->kill();
        displayStopScript->deleteLater();
    }
}

} // namespace SDDM
