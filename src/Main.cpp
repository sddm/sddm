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
#include "LockFile.h"
#include "SessionManager.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QQuickView>
#include <QQmlContext>
#else
#include <QDeclarativeView>
#include <QDeclarativeContext>
#endif


#include <iostream>

using namespace SDE;

int main(int argc, char **argv) {
    QString display = ":0";
    // grab DISPLAY environment variable if set
    if (getenv("DISPLAY") != nullptr)
        display = getenv("DISPLAY");

    QString configPath = "/etc/sddm.conf";
    QString themePath = "";
    bool testing = false;
    // parse command line arguments
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-t") == 0) {
            testing = true;
            if (i < argc - 1)
                themePath = argv[i + 1];
        } else if ((strcmp(argv[i], "-c") == 0) && (i < argc - 1)) {
            configPath = argv[i + 1];
        }
    }

    // create configuration instance
    Configuration configuration(configPath);

    // create lock file
    LockFile lock(Configuration::instance()->lockFile(), testing);
    if (!lock.success())
        return 1;

    // set theme
    if (themePath.isEmpty())
        themePath = Configuration::instance()->themesDir() + "/" + Configuration::instance()->currentTheme();

    bool first = true;

    while (true) {
        // create cookie
        Cookie cookie;
        // create display manager
        DisplayManager displayManager;
        displayManager.setDisplay(display);
        displayManager.setCookie(cookie);
        // start the display manager, except when in test mode
        if ((testing == false) && (displayManager.start() == false)) {
            qCritical() << "error: could not start display manager.";
            return 1;
        }
        // create session manager
        SessionManager sessionManager;
        sessionManager.setDisplay(display);
        sessionManager.setCookie(cookie);
        // auto login
        if (first && !Configuration::instance()->autoUser().isEmpty()) {
            // restart flag
            first = false;
            // auto login
            sessionManager.autoLogin();
            // restart
            continue;
        }
        // create application
        QApplication app(argc, argv);
        // create declarative view
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        // add session manager to context
        QQmlContext *context = view.rootContext();
#else
        QDeclarativeView view;
        view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
        // add session manager to context
        QDeclarativeContext *context = view.rootContext();
#endif
        context->setContextProperty("sessionManager", &sessionManager);
        // load qml file
        view.setSource(QUrl::fromLocalFile(QString("%1/Main.qml").arg(themePath)));
        // show view
        if (!testing) {
            // close view on successful login
            QObject::connect(&sessionManager, SIGNAL(success()), &view, SLOT(close()));
            // show view
            view.show();
            view.setGeometry(QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen()));
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
    // return success
    return 0;
}
