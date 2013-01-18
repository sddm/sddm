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

#include "Authenticator.h"
#include "Configuration.h"
#include "DisplayManager.h"
#include "SessionManager.h"

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <signal.h>

#include <QApplication>
#include <QDeclarativeView>
#include <QDeclarativeContext>

using namespace SDE;

bool createLock(const char *path) {
    std::ifstream test(path);
    // check lock file
    if (test) {
        // if exists, read pid from it
        pid_t pid = 0;
        test >> pid;
        test.close();
        // check if another process with the pid is running
        if (pid > 0) {
            int ret = kill(pid, 0);
            if (ret == 0 || errno == EPERM) {
                // print error message
                std::cerr << "error: another instance of the program is running with PID " << pid << "." << std::endl;
                // return fail
                return false;
            }
        }
        // remove lock file
        remove(path);
    }
    // create lock
    std::ofstream lock(path, std::ios_base::out);
    // if can not create lock, return
    if (!lock) {
        std::cerr << "error: could not create lock file." << std::endl;
        return false;
    }
    // write pid into the file
    lock << getpid() << std::endl;
    // close file
    lock.close();
    // return success
    return true;
}

void removeLock(const char *path) {
    remove(path);
}

int main(int argc, char **argv) {
    QString configPath = "/etc/sddm.conf";
    QString themePath = "";
    bool testing = false;
    // parse command line arguments
    for (int i = 0; i < argc; ++i) {
        if ((strcmp(argv[i], "-t") == 0) && (i < argc - 1)) {
            themePath = argv[i + 1];
            testing = true;
        } else if ((strcmp(argv[i], "-c") == 0) && (i < argc - 1)) {
            configPath = argv[i + 1];
        }
    }

    // create configuration instance
    Configuration configuration(configPath);

    // set theme
    if (!testing)
        themePath = Configuration::instance()->themesDir() + "/" + Configuration::instance()->currentTheme();

    // create lock, except when in test mode
    const char *lockFile = Configuration::instance()->lockFile().toStdString().c_str();
    if ((testing == false) && (createLock(lockFile) == false))
        return 1;

    while (true) {
        // create cookie
        Cookie cookie;
        // create display manager
        DisplayManager displayManager;
        displayManager.setDisplay(":0");
        displayManager.setCookie(cookie);
        // start the display manager, except when in test mode
        if ((testing == false) && (displayManager.start() == false)) {
            std::cerr << "error: could not start display manager." << std::endl;
            return 1;
        }
        // create session manager
        SessionManager sessionManager;
        sessionManager.setDisplay(":0");
        sessionManager.setCookie(cookie);
        // create application
        QApplication app(argc, argv);
        // create declarative view
        QDeclarativeView view;
        view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
        // add session manager to context
        QDeclarativeContext *context = view.rootContext();
        context->setContextProperty("sessionManager", &sessionManager);
        // load qml file
        view.setSource(QUrl::fromLocalFile(QString("%1/Main.qml").arg(themePath)));
        // show view
        if (!testing) {
            // close view on successful login
            QObject::connect(&sessionManager, SIGNAL(success()), &view, SLOT(close()));
            // show view
            view.show();
            view.move(0, 0);
            view.resize(displayManager.width(), displayManager.height());
            // execute application
            app.exec();
        } else {
            // show application
            view.showFullScreen();
            // execute application
            app.exec();
            // in test mode, we want to quit when the window is closed
            break;
        }
    }
    // remove lock
    if (!testing)
        removeLock(lockFile);
    // return success
    return 0;
}
