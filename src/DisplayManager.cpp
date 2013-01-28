/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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

#include <QDebug>
#include <QProcess>
#include <QThread>

#include <X11/Xlib.h>

#include <unistd.h>

namespace SDE {
    class DisplayManagerPrivate {
    public:
        Cookie cookie;
        QString displayName { ":0" };
        QProcess *serverProcess { nullptr };
    };

    DisplayManager::DisplayManager() : d(new DisplayManagerPrivate()) {
    }

    DisplayManager::~DisplayManager() {
        stop();
        delete d;
    }

    void DisplayManager::setCookie(const Cookie &cookie) {
        d->cookie = cookie;
    }

    void DisplayManager::setDisplay(const QString &display) {
        d->displayName = display;
    }

    bool DisplayManager::start() {
        if (d->serverProcess)
            return false;
        // path of the server
        char *authPath = strdup(Configuration::instance()->authFile().toStdString().c_str());
        // set environment variables
        setenv("DISPLAY", d->displayName.toStdString().c_str(), 1);
        setenv("XAUTHORITY", authPath, 1);
        // remove authority file
        remove(authPath);
        // add cookie
        d->cookie.add(d->displayName, authPath);
        // create arguments array
        QStringList arguments;
        arguments << d->displayName << Configuration::instance()->serverArgs();
        arguments << QString("-auth") << authPath;
        // create process
        d->serverProcess = new QProcess();
        // start the process
        d->serverProcess->start(Configuration::instance()->serverPath(), arguments);
        d->serverProcess->waitForStarted();
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
        if (!d->serverProcess)
            return false;
        // send terminate signal
        d->serverProcess->terminate();
        d->serverProcess->waitForFinished(1000);
        // send kill signal
        d->serverProcess->kill();
        d->serverProcess->waitForFinished();
        // clean up
        delete d->serverProcess;
        d->serverProcess = nullptr;
        // return success
        return true;
    }
}
