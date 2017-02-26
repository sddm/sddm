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

#include <QtCore/QCoreApplication>

#include "Configuration.h"
#include "../daemon/SignalHandler.h"

#include "greeter.h"
#include "launcher.h"
#include "xorgserver.h"
#include "usersession.h"

Launcher::Launcher(QObject *parent)
    : QObject(parent)
    , m_rootless(false)
    , m_testMode(false)
    , m_server(new XorgServer(this))
    , m_process(nullptr)
    , m_signalHandler(new SDDM::SignalHandler(this))
{
    // Tear down everything when quitting
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        stop();
    });

    // Stop when a signal is received
    SDDM::SignalHandler::initialize();
    connect(m_signalHandler, SIGNAL(sighupReceived()),
            QCoreApplication::instance(), SLOT(quit()));
    connect(m_signalHandler, SIGNAL(sigintReceived()),
            QCoreApplication::instance(), SLOT(quit()));
    connect(m_signalHandler, SIGNAL(sigtermReceived()),
            QCoreApplication::instance(), SLOT(quit()));
}

bool Launcher::isRootless() const
{
    return m_rootless;
}

bool Launcher::isTestModeEnabled() const
{
    return m_testMode;
}

void Launcher::setTestModeEnabled(bool on)
{
    m_testMode = on;
}

QString Launcher::socket() const
{
    return m_socket;
}

void Launcher::setSocket(const QString &socket)
{
    m_socket = socket;
}

QString Launcher::session() const
{
    return m_session;
}

void Launcher::setSession(const QString &session)
{
    m_session = session;
}

bool Launcher::start()
{
    if (m_session.isEmpty()) {
        m_process = new Greeter(this);
    } else {
        m_process = new UserSession(this);
        qobject_cast<UserSession *>(m_process)->setSession(m_session);

        // Stop Xorg and quit when the user session stops
        connect(m_process, &Process::stopped, this, [this] {
            m_server->stop();
            QCoreApplication::instance()->quit();
        });
    }

    // Stop greeter/user session when the server stops
    connect(m_server, &XorgServer::stopped, m_process, &Process::stop);

    if (!m_server->start())
        return false;
    m_process->setEnvironment(QLatin1String("DISPLAY"), m_server->display());
    m_process->setEnvironment(QLatin1String("XAUTHORITY"), m_server->authPath());
    return m_process->start();
}

void Launcher::stop()
{
    if (m_process) {
        if (m_process->isRunning())
            m_process->stop();
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_server->stop();
}
