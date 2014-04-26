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

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include "WaylandIntegration.h"

namespace SDDM {
    WaylandIntegration::WaylandIntegration() : fullScreenShell(new QtWayland::_wl_fullscreen_shell()) {
        // Platform native interface
        QPlatformNativeInterface *native =
                QGuiApplication::platformNativeInterface();
        Q_ASSERT(native);

        // Get Wayland display
        struct ::wl_display *display = static_cast<struct ::wl_display *>(
                    native->nativeResourceForIntegration("display"));
        Q_ASSERT(display);

        // Wayland registry
        struct ::wl_registry *registry = wl_display_get_registry(display);
        Q_ASSERT(registry);

        // Initialize interfaces
        wl_registry_add_listener(registry, &WaylandIntegration::listener, this);
    }

    WaylandIntegration::~WaylandIntegration() {
        delete fullScreenShell;
    }

    void WaylandIntegration::presentWindow(QWindow *window) {
        QPlatformNativeInterface *native =
                QGuiApplication::platformNativeInterface();

        struct ::wl_surface *surface = static_cast<struct ::wl_surface *>(
                    native->nativeResourceForWindow("surface", window));
        struct ::wl_output *output = static_cast<struct ::wl_output *>(
                    native->nativeResourceForScreen("output", window->screen()));
        fullScreenShell->present_surface(surface,
                                         QtWayland::_wl_fullscreen_shell::present_method_default,
                                         output);
    }

    void WaylandIntegration::handleGlobal(void *data,
                                          struct ::wl_registry *registry,
                                          uint32_t id,
                                          const char *interface,
                                          uint32_t version) {
        Q_UNUSED(version);

        WaylandIntegration *self = static_cast<WaylandIntegration *>(data);
        if (!self) {
            qWarning() << "Unable to cast data to WaylandIntegration pointer in handleGlobal";
            return;
        }

        if (strcmp(interface, "_wl_fullscreen_shell") == 0)
            self->fullScreenShell->init(registry, id);
    }

    void WaylandIntegration::handleGlobalRemove(void *data,
                                                struct ::wl_registry *registry,
                                                uint32_t name) {
        Q_UNUSED(name);
        Q_UNUSED(registry);

        WaylandIntegration *self = static_cast<WaylandIntegration *>(data);
        if (!self) {
            qWarning() << "Unable to cast data to WaylandIntegration pointer in handleGlobalRemove";
            return;
        }

        qDebug() << "Release _wl_fullscreen_shell";
        self->fullScreenShell->release();
    }

    const struct wl_registry_listener WaylandIntegration::listener = {
        WaylandIntegration::handleGlobal,
        WaylandIntegration::handleGlobalRemove
    };
}
