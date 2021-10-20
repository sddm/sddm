/***************************************************************************
* Copyright (c) 2021 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#include "waylandhelper.h"
#include "waylandsocketwatcher.h"
#include "VirtualTerminal.h"

#include <fcntl.h>
#include <unistd.h>

namespace SDDM {

WaylandHelper::WaylandHelper(QObject *parent)
    : QObject(parent)
    , m_environment(QProcessEnvironment::systemEnvironment())
    , m_watcher(new WaylandSocketWatcher(this))
{
}

QProcessEnvironment WaylandHelper::environment() const
{
    return m_environment;
}

void WaylandHelper::setEnvironment(const QProcessEnvironment &env)
{
    m_environment = env;
}

bool WaylandHelper::startCompositor(const QString &cmd)
{
    m_watcher->start();
    return startProcess(cmd, &m_serverProcess);
}

void WaylandHelper::stop()
{
    m_watcher->stop();
    if (m_serverProcess) {
        qInfo("Stopping server...");
        m_serverProcess->terminate();
        if (!m_serverProcess->waitForFinished(5000))
            m_serverProcess->kill();
        m_serverProcess->deleteLater();
        m_serverProcess = nullptr;
    }
}

bool WaylandHelper::startProcess(const QString &cmd, QProcess **p)
{
    auto *process = new QProcess(this);
    process->setProcessEnvironment(m_environment);
    process->setInputChannelMode(QProcess::ForwardedInputChannel);
    connect(process, &QProcess::readyReadStandardError, this, [process] {
        qWarning() << process->readAllStandardError();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] {
        qInfo() << process->readAllStandardOutput();
    });
    qDebug() << "Starting Wayland process" << cmd << m_environment.value(QStringLiteral("USER"));
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process, [](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "wayland compositor finished" << exitCode << exitStatus;
        if (exitCode != 0 || exitStatus != QProcess::NormalExit)
            QCoreApplication::instance()->quit();
    });

    auto args = QProcess::splitCommand(cmd);
    const auto program = args.takeFirst();
    process->start(program, args);
    if (!process->waitForStarted(10000)) {
        qWarning("Failed to start \"%s\": %s",
                 qPrintable(cmd),
                 qPrintable(process->errorString()));
        return false;
    }

    if (p)
        *p = process;

    qDebug() << "started succesfully" << cmd;
    return true;
}

void WaylandHelper::startGreeter(const QString &cmd)
{
    auto args = QProcess::splitCommand(cmd);

    QProcess *process = new QProcess(this);
    process->setProgram(args.takeFirst());
    process->setArguments(args);
    process->setProcessEnvironment(m_environment);
    connect(process, &QProcess::readyReadStandardError, this, [process] {
        qWarning() << process->readAllStandardError();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] {
        qInfo() << process->readAllStandardOutput();
    });
    startGreeter(process);
}

void WaylandHelper::startGreeter(QProcess *process)
{
    if (m_watcher->status() == WaylandSocketWatcher::Started) {
        process->start();
    } else if (m_watcher->status() == WaylandSocketWatcher::Failed) {
        Q_EMIT failed();
    } else {
        connect(m_watcher, &WaylandSocketWatcher::failed, this, &WaylandHelper::failed);
        connect(m_watcher, &WaylandSocketWatcher::started, this, [this, process] {
            m_watcher->stop();
            process->start();
        });
    }
}

} // namespace SDDM
