/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QtGui/QGuiApplication>
#include <GreenIsland/Server/HomeApplication>

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")) {
        qCritical("No D-Bus session bus available, please run the compositor with dbus-launch.");
        return 1;
    }

    // Make sure to pick the right EGL integration should someone
    // run this on X11 for testing
    qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SDDM Wayland Compositor"));
    app.setOrganizationName(QStringLiteral("SDDM"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    app.setFallbackSessionManagementEnabled(false);
#endif

    GreenIsland::Server::HomeApplication home;
    if (!home.loadUrl(QUrl(QStringLiteral("qrc:/qml/Compositor.qml"))))
        qFatal("Fatal: unable to load compositor QML code");

    return app.exec();
}
