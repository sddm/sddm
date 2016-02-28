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

#include "waylandsocketwatcher.h"
#include "waylanduserhelper.h"

#include <unistd.h>

namespace SDDM {

StartupEvent::StartupEvent()
    : QEvent(StartupEventType)
{
}

WaylandUserHelper::WaylandUserHelper(int fd, const QString &serverCmd,
                                     const QString &clientCmd,
                                     QObject *parent)
    : QObject(parent)
    , m_fd(fd)
    , m_serverCmd(serverCmd)
    , m_clientCmd(clientCmd)
{
}

bool WaylandUserHelper::start()
{
    // Start server process
    if (!startServer())
        return false;

    // Start client process
    if (!startClient())
        return false;

    return true;
}

void WaylandUserHelper::stop()
{
    if (m_clientProcess) {
        qInfo("Stopping client...");
        m_clientProcess->terminate();
        if (!m_clientProcess->waitForFinished(5000))
            m_clientProcess->kill();
        m_clientProcess->deleteLater();
        m_clientProcess = nullptr;
    }

    if (m_serverProcess) {
        qInfo("Stopping server...");
        m_serverProcess->terminate();
        if (!m_serverProcess->waitForFinished(5000))
            m_serverProcess->kill();
        m_serverProcess->deleteLater();
        m_serverProcess = nullptr;
    }
}

void WaylandUserHelper::customEvent(QEvent *event)
{
    if (event->type() == StartupEventType) {
        if (!start())
            QCoreApplication::exit(EXIT_FAILURE);
    }
}

bool WaylandUserHelper::startProcess(const QString &cmd,
                                     const QProcessEnvironment &env,
                                     QProcess **p)
{
    auto args = QProcess::splitCommand(cmd);
    const auto program = args.takeFirst();

    auto *process = new QProcess(this);
    //process->setInputChannelMode(QProcess::ForwardedInputChannel);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->setProcessEnvironment(env);

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

bool WaylandUserHelper::startServer()
{
    // Server environment
    QProcessEnvironment serverEnv = QProcessEnvironment::systemEnvironment();

    // Start the server process
    qInfo("Running server: %s", qPrintable(m_serverCmd));
    if (!startProcess(m_serverCmd, serverEnv, &m_serverProcess))
        return false;

    // Wait until the socket is available
    WaylandSocketWatcher watcher;
    QEventLoop loop;
    connect(&watcher, &WaylandSocketWatcher::started, &loop, &QEventLoop::quit);
    connect(&watcher, &WaylandSocketWatcher::failed, &loop, &QEventLoop::quit);
    watcher.start();
    loop.exec();

    if (watcher.status() == WaylandSocketWatcher::Started) {
        // Send the display name to the caller
        if (m_fd > 0) {
            auto socketPath = watcher.socketPath().toLocal8Bit();
            ::write(m_fd, socketPath.constData(), socketPath.length());
        }

        return true;
    }

    return false;
}

bool WaylandUserHelper::startClient()
{
    // Client environment
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), m_display);
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"), QStringLiteral("fullscreen-shell-v1"));

    // Start the client process
    qInfo("Running client: %s", qPrintable(m_clientCmd));
    if (!startProcess(m_clientCmd, env, &m_clientProcess))
        return false;

    return true;
}

} // namespace SDDM
