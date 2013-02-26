/***************************************************************************
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

#include "DisplayManager.h"

#include "Configuration.h"
#include "Cookie.h"
#include "Util.h"

#include <QDebug>

#include <X11/Xlib.h>

namespace SDE {
    class DisplayManagerPrivate {
    public:
        QString cookie { "" };
        QString displayName { ":0" };
        pid_t pid { 0 };
    };

    DisplayManager::DisplayManager() : d(new DisplayManagerPrivate()) {
    }

    DisplayManager::~DisplayManager() {
        stop();
        // clean up
        delete d;
    }

    void DisplayManager::setCookie(const QString &cookie) {
        d->cookie = cookie;
    }

    void DisplayManager::setDisplay(const QString &display) {
        d->displayName = display;
    }

    bool DisplayManager::start() {
        if (d->pid)
            return false;
        // path of the server
        char *authPath = strdup(Configuration::instance()->authFile().toStdString().c_str());
        setenv("XAUTHORITY", authPath, 1);
        // remove authority file
        remove(authPath);
        // add cookie
        Cookie::add(d->cookie, d->displayName, authPath);
        // create arguments array
        QStringList arguments;
        arguments << d->displayName << Configuration::instance()->serverArgs();
        arguments << QString("-auth") << authPath;
        // create process
        d->pid = Util::execute(Configuration::instance()->serverPath(), arguments, [] {});
        // try to connect to the display server
        Display *display = nullptr;
        // try to open the display
        for (int i = 0; i < 5; ++i) {
            // sleep for a second
            sleep(1);
            // try to open the display
            if ((display = XOpenDisplay(d->displayName.toStdString().c_str())) != nullptr)
                break;
        }
        // quit, if display can not be opened
        if (display == nullptr) {
            // print error message
            qCritical() << "error: could not connect to the X server.";
            // return fail
            return false;
        }
        // close display
        XCloseDisplay(display);
        // return success
        return true;
    }

    bool DisplayManager::stop() {
        if (!d->pid)
            return false;
        // terminate server
        Util::terminate(d->pid);
        // reset server pid
        d->pid = 0;
        // return success
        return true;
    }
}
