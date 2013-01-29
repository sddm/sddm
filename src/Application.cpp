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

#include "Application.h"

#include "Configuration.h"
#include "Cookie.h"
#include "DisplayManager.h"
#include "LockFile.h"
#include "SessionManager.h"
#include "Util.h"

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

namespace SDE {
    class ApplicationPrivate {
    public:
        ApplicationPrivate() {
        }

        ~ApplicationPrivate() {
            delete configuration;
        }

        Configuration *configuration { nullptr };
        QStringList arguments;
        int argc { 0 };
        char **argv { nullptr };
    };

    Application::Application(int argc, char **argv) : d(new ApplicationPrivate()) {
        d->argc = argc;
        d->argv = argv;
        // convert arguments
        for (int i = 1; i < argc; ++i)
            d->arguments << QString(argv[i]);
    }

    Application::~Application() {
        delete d;
    }

    const QStringList &Application::arguments() const {
        return d->arguments;
    }

    void Application::init(const QString &config) {
        d->configuration = new Configuration(config);
    }

    void Application::test(const QString &theme) {
        QString testTheme = QString("%1/Main.qml").arg(theme);
        QString currentTheme = QString("%1/%2/Main.qml").arg(Configuration::instance()->themesDir()).arg(Configuration::instance()->currentTheme());

        // create application
        QApplication app(d->argc, d->argv);
        // create declarative view
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView);
#else
        QDeclarativeView view;
        view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
#endif
        // add session manager to context
        SessionManager sessionManager;
        view.rootContext()->setContextProperty("sessionManager", &sessionManager);
        // load theme
        if (!theme.isEmpty())
            view.setSource(QUrl::fromLocalFile(testTheme));
        else
            view.setSource(QUrl::fromLocalFile(currentTheme));
        // show application
        view.showFullScreen();
        // execute application
        app.exec();
    }

    void Application::run() {
        QString currentTheme = QString("%1/%2/Main.qml").arg(Configuration::instance()->themesDir()).arg(Configuration::instance()->currentTheme());
        // create lock file
        LockFile lock(Configuration::instance()->lockFile());
        if (!lock.success())
            return;

        bool first = true;

        while (true) {
            // set DISPLAY environment variable if not set
            if (getenv("DISPLAY") == nullptr)
                setenv("DISPLAY", ":0", 1);
            // get DISPLAY
            QString display = getenv("DISPLAY");

            // generate cookie
            char cookie[33] { 0 };
            Cookie::generate(cookie);

            // create display manager
            DisplayManager displayManager;
            displayManager.setDisplay(display);
            displayManager.setCookie(cookie);

            // start the display manager
            if (!displayManager.start()) {
                qCritical() << "error: could not start display manager.";
                return;
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

            // execute user interface in a seperate process
            // this is needed because apperantly we can create a QApplication
            // instance multiple times in the same process. if this changes in
            // a future version of Qt, this workaround should be removed.
            pid_t pid = Util::execute([&] {
                // create application
                QApplication app(d->argc, d->argv);
                // create declarative view
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                QQuickView view;
                view.setResizeMode(QQuickView::SizeRootObjectToView);
#else
                QDeclarativeView view;
                view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
#endif
                // add session manager to context
                view.rootContext()->setContextProperty("sessionManager", &sessionManager);
                // load qml file
                view.setSource(QUrl::fromLocalFile(currentTheme));
                // close view on successful login
                QObject::connect(&sessionManager, SIGNAL(success()), &view, SLOT(close()));
                // show view
                view.show();
                view.setGeometry(QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen()));
                // execute application
                app.exec();
            });
            // wait for process to end
            Util::wait(pid);
        }
    }
}
