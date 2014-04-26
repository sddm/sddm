/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef WAYLANDINTEGRATION_H
#define WAYLANDINTEGRATION_H

#include "qwayland-fullscreen-shell.h"

namespace SDDM {
    class WaylandIntegration {
    public:
        WaylandIntegration();
        ~WaylandIntegration();

        void presentWindow(QWindow *window);

        QtWayland::_wl_fullscreen_shell *fullScreenShell;

    private:
        static void handleGlobal(void *data, struct ::wl_registry *registry,
                                 uint32_t id, const char *interface,
                                 uint32_t version);
        static void handleGlobalRemove(void *data,
                                       struct ::wl_registry *registry,
                                       uint32_t name);

        static const struct wl_registry_listener listener;
    };
}

#endif // WAYLANDINTEGRATION_H
