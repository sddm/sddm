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

#include "Configuration.h"
#include "Constants.h"
#include "GreeterApp.h"
#include "GreeterProxy.h"
#include "KeyboardModel.h"
#include "ScreenModel.h"
#include "SessionModel.h"
#include "ThemeConfig.h"
#include "ThemeMetadata.h"
#include "UserModel.h"

#ifdef USE_QT5
#include "MessageHandler.h"

#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#else
#include <QApplication>
#include <QDeclarativeView>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#endif
#include <QDebug>

#include <iostream>

namespace SDDM {
    QString parameter(const QStringList &arguments, const QString &key, const QString &defaultValue) {
        int index = arguments.indexOf(key);

        if ((index < 0) || (index >= arguments.size() - 1))
            return defaultValue;

        QString value = arguments.at(index + 1);

        if (value.startsWith("-") || value.startsWith("--"))
            return defaultValue;

        return value;
    }

    GreeterApp *GreeterApp::self = nullptr;

    GreeterApp::GreeterApp(int argc, char **argv) :
#ifdef USE_QT5
    QGuiApplication(argc, argv)
#else
    QApplication(argc, argv)
#endif
    {
        // point instance to this
        self = this;

        // Parse arguments
        bool testing = false;

        if (arguments().contains("--test"))
            testing = true;

        // get socket name
        QString socket = parameter(arguments(), "--socket", "");

        // get theme path
        QString themePath = parameter(arguments(), "--theme", "");

        // Initialize
    #ifdef USE_QT5
        // create view
        m_view = new QQuickView();
        m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    #else
        // create view
        m_view = new QDeclarativeView();
        m_view->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    #endif

        m_view->engine()->addImportPath(IMPORTS_INSTALL_DIR);

        // create configuration instance
        m_configuration = new Configuration(CONFIG_FILE);

        // read theme metadata
        m_metadata = new ThemeMetadata(QString("%1/metadata.desktop").arg(themePath));

        // get theme config file
        QString configFile = QString("%1/%2").arg(themePath).arg(m_metadata->configFile());

        // read theme config
        m_themeConfig = new ThemeConfig(configFile);

        // create models

        m_sessionModel = new SessionModel();
        m_screenModel = new ScreenModel();
        m_userModel = new UserModel();
        m_proxy = new GreeterProxy(socket);
        m_keyboard = new KeyboardModel();

        if(!testing && !m_proxy->isConnected()) {
            qCritical() << "Cannot connect to the daemon - is it running?";
            exit(EXIT_FAILURE);
        }

        m_proxy->setSessionModel(m_sessionModel);

        // connect proxy signals
        QObject::connect(m_proxy, SIGNAL(loginSucceeded()), m_view, SLOT(close()));

        // set context properties
        m_view->rootContext()->setContextProperty("sessionModel", m_sessionModel);
        m_view->rootContext()->setContextProperty("screenModel", m_screenModel);
        m_view->rootContext()->setContextProperty("userModel", m_userModel);
        m_view->rootContext()->setContextProperty("config", *m_themeConfig);
        m_view->rootContext()->setContextProperty("sddm", m_proxy);
        m_view->rootContext()->setContextProperty("keyboard", m_keyboard);

        // Set numlock
        if (m_keyboard->enabled()) {
            if (m_configuration->numlock() == Configuration::NUM_SET_ON)
                m_keyboard->setNumLockState(true);
            else if (m_configuration->numlock() == Configuration::NUM_SET_OFF)
                m_keyboard->setNumLockState(false);
        }

        // get theme main script
        QString mainScript = QString("%1/%2").arg(themePath).arg(m_metadata->mainScript());

        // set main script as source
        m_view->setSource(QUrl::fromLocalFile(mainScript));

        // connect screen update signals
        connect(m_screenModel, SIGNAL(primaryChanged()), this, SLOT(show()));

        show();
#ifndef USE_QT5
        m_view->showFullScreen();
#endif
    }

    void GreeterApp::show() {
        m_view->setGeometry(m_screenModel->geometry());
#ifdef USE_QT5
        m_view->showFullScreen();
#endif
    }

}

int main(int argc, char **argv) {
#ifdef USE_QT5
    // install message handler
    qInstallMessageHandler(SDDM::MessageHandler);
#endif
    QStringList arguments;

    for (int i = 0; i < argc; i++)
        arguments << argv[i];

    if (arguments.contains(QLatin1String("--help")) || arguments.contains(QLatin1String("-h"))) {
        std::cout << "Usage: " << argv[0] << " [options] [arguments]\n"
                     "Options: \n"
                     "  --theme <theme path>       Set greeter theme\n"
                     "  --socket <socket name>     Set socket name\n"
                     "  --test                     Testing mode" << std::endl;

        return EXIT_FAILURE;
    }

    SDDM::GreeterApp app(argc, argv);

    return app.exec();
}
