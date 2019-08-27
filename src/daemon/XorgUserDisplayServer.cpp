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

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "Seat.h"
#include "XorgUserDisplayServer.h"

namespace SDDM {

XorgUserDisplayServer::XorgUserDisplayServer(Display *parent)
    : DisplayServer(parent)
{
}

XorgUserDisplayServer::~XorgUserDisplayServer()
{
    stop();
}

QString XorgUserDisplayServer::sessionType() const
{
    return QStringLiteral("x11");
}

void XorgUserDisplayServer::setDisplayName(const QString &displayName)
{
    m_display = displayName;
}

QString XorgUserDisplayServer::command(Display *display)
{
    QStringList args;

    if (daemonApp->testing()) {
        args << mainConfig.X11.XephyrPath.get()
             << QStringLiteral("-br")
             << QStringLiteral("-screen") << QStringLiteral("800x600");
    } else {
        args << mainConfig.X11.ServerPath.get()
             << mainConfig.X11.ServerArguments.get().split(QLatin1Char(' '), Qt::SkipEmptyParts)
             << QStringLiteral("-background") << QStringLiteral("none")
             << QStringLiteral("-seat") << display->seat()->name()
             << QStringLiteral("-noreset")
             << QStringLiteral("-keeptty")
             << QStringLiteral("-novtswitch")
             << QStringLiteral("-verbose") << QStringLiteral("3");
    }

    return args.join(QLatin1Char(' '));
}

bool XorgUserDisplayServer::start()
{
    // Check flag
    if (m_started)
        return false;

    // Set flag
    m_started = true;
    emit started();

    return true;
}

void XorgUserDisplayServer::stop()
{
    // Check flag
    if (!m_started)
        return;

    // Reset flag
    m_started = false;
    emit stopped();
}

void XorgUserDisplayServer::finished()
{
}

void XorgUserDisplayServer::setupDisplay()
{
}

} // namespace SDDM
