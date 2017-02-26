/*
 * This file is part of SDDM.
 *
 * Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "Configuration.h"

#include "launcher.h"
#include "usersession.h"

UserSession::UserSession(Launcher *parent)
    : Process(parent)
    , m_process(new QProcess(this))
{
    // Relay process messages
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        qDebug() << "Session output:" << m_process->readAllStandardOutput().constData();
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        qDebug() << "Session errors:" << m_process->readAllStandardError().constData();
    });

    // Stop when the process has finished
    connect(m_process, SIGNAL(finished(int)), this, SLOT(finished()));
}

QString UserSession::session() const
{
    return m_session;
}

void UserSession::setSession(const QString &session)
{
    m_session = session;
}

bool UserSession::start()
{
    // Process environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QLatin1String("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
    for (const QString &key : m_env.keys())
        env.insert(key, m_env[key]);
    m_process->setProcessEnvironment(env);

    // Run command
    m_process->start(m_session);
    qDebug() << "Running:" << qPrintable(m_session);

    if (m_process->state() == QProcess::NotRunning) {
        qCritical("Failed to run greeter");
        return false;
    }

    if (!m_process->waitForStarted()) {
        qCritical("Failed to start greeter");
        return false;
    }

    qInfo("Session started");
    emit started();
    m_started = true;

    return true;
}

void UserSession::stop()
{
    // Terminate the process when stop is requested
    m_process->terminate();
    if (!m_process->waitForFinished())
        m_process->kill();
}

void UserSession::finished()
{
    qInfo("Session stopped");
    emit stopped();
    m_started = false;
}
