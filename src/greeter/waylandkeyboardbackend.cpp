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

#include <QDir>

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#include "KeyboardLayout.h"
#include "waylandkeyboardbackend.h"

namespace SDDM {

WaylandKeyboardBackend::WaylandKeyboardBackend(KeyboardModelPrivate *kmp)
    : KeyboardBackend(kmp)
{
}

WaylandKeyboardBackend::~WaylandKeyboardBackend()
{
}

void WaylandKeyboardBackend::init()
{
    d->layouts.clear();

    QDir dir(QStringLiteral("/usr/share/X11/xkb/symbols"));
    auto entries = dir.entryList(QDir::Files);
    for (const auto &entry : qAsConst(entries))
        d->layouts << new KeyboardLayout(entry, entry);
}

void WaylandKeyboardBackend::disconnect()
{
}

void WaylandKeyboardBackend::sendChanges()
{
}

void WaylandKeyboardBackend::dispatchEvents()
{
}

void WaylandKeyboardBackend::connectEventsDispatcher(KeyboardModel *model)
{
    Q_UNUSED(model);
}

} // namespace SDDM
