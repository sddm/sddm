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

#include <X11/Xlib.h>

#include <iostream>

#include <unistd.h>
#include <wait.h>

namespace SDE {
    class DisplayManagerPrivate {
    public:
        Cookie cookie;
        int processID { 0 };
        bool started { false };
        QString displayName { ":0" };
    };

    DisplayManager::DisplayManager() : d(new DisplayManagerPrivate()) {
    }

    DisplayManager::~DisplayManager() {
        if (d->started)
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
        if (d->started)
            stop();
        // path of the server
        char *authPath = strdup(Configuration::instance()->authFile().toStdString().c_str());
        // set environment variables
        setenv("DISPLAY", d->displayName.toStdString().c_str(), 1);
        setenv("XAUTHORITY", authPath, 1);
        // remove authority file
        remove(authPath);
        // add cookie
        d->cookie.add(d->displayName, authPath);
        // fork the process
        d->processID = fork();
        // check for result
        if (d->processID == -1) {
            // fork failed, return fail
            return false;
        } else if (d->processID == 0) {
            // we are in the child process, replace it with display server process
            // create arguments array
            int size = Configuration::instance()->serverArgs().size();
            char **arguments = (char **)malloc(sizeof(void *) * (size + 4));
            memset(arguments, 0, sizeof(void *) * (size + 4));
            // first argument is server path
            int index = 0;
            arguments[index++] = strdup(Configuration::instance()->serverPath().toStdString().c_str());
            // copy arguments coming from configuration
            for (const QString &arg: Configuration::instance()->serverArgs())
                arguments[index++] = strdup(arg.toStdString().c_str());
            // add mandatory auth option
            arguments[index++] = (char *)"-auth";
            arguments[index++] = authPath;
            // end with null pointer
            arguments[index++] = nullptr;
            // execute command
            execv(arguments[0], (char **)arguments);
            // if execv returns, this means we failed, show error message
            std::cerr << "error: could not start X." << std::endl;
            // and exit
            _exit(1);
        }
        // main process, connect to the display server
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
            std::cerr << "error: could not connect to the X server." << std::endl;
            // return fail
            return false;
        }
        // close display
        XCloseDisplay(display);
        // set flag
        d->started = true;
        // return success
        return true;
    }

    void DisplayManager::stop() {
        if (!d->started)
            return;

        // check process id
        if (d->processID < 0)
            return;

        // send SIGTERM to server
        killpg(d->processID, SIGTERM);

        // wait for x server to shut down
        for (int i = 0; i < 10; ++i) {
            // sleep for a second
            sleep(1);
            // try to open display
            Display *display = nullptr;
            if ((display = XOpenDisplay(d->displayName.toStdString().c_str())) == nullptr) {
                d->started = false;
                return;
            }
            // close display
            XCloseDisplay(display);
        }

        // send kill signal
        killpg(d->processID, SIGKILL);
        // reset flag
        d->started = false;
    }
}
