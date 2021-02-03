/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "WaylandDisplayServer.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "DisplayManager.h"
#include "Seat.h"
#include "SignalHandler.h"
#include "VirtualTerminal.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QUuid>

#include <pwd.h>
#include <unistd.h>

namespace SDDM
{
WaylandDisplayServer::WaylandDisplayServer(Display *parent)
    : DisplayServer(parent)
{
    m_display = QStringLiteral("wayland");
}

WaylandDisplayServer::~WaylandDisplayServer()
{
    stop();
}

const QString &WaylandDisplayServer::display() const
{
    return m_display;
}

QString WaylandDisplayServer::sessionType() const
{
    return QStringLiteral("wayland");
}

bool WaylandDisplayServer::start()
{
    // check flag
    if (m_started)
        return false;

    if (!daemonApp->testing()) {
        // setup vt
        VirtualTerminal::jumpToVt(displayPtr()->terminalId(), true);
    }
    // set flag
    m_started = true;
    emit started();

    return true;
}

void WaylandDisplayServer::stop()
{
    // check flag
    if (!m_started)
        return;

    // reset flag
    m_started = false;

    // emit signal
    emit stopped();
}

void WaylandDisplayServer::finished()
{
}

}

