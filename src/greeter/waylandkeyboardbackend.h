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

#ifndef WAYLANDKEYBOARDBACKEND_H
#define WAYLANDKEYBOARDBACKEND_H

#include "KeyboardBackend.h"

namespace SDDM {

class WaylandKeyboardBackend : public KeyboardBackend
{
public:
    WaylandKeyboardBackend(KeyboardModelPrivate *kmp);
    virtual ~WaylandKeyboardBackend();

    void init() override;
    void disconnect() override;
    void sendChanges() override;
    void dispatchEvents() override;

    void connectEventsDispatcher(KeyboardModel *model) override;
};

} // namespace SDDM

#endif // WAYLANDKEYBOARDBACKEND_H
