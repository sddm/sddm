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
#include "GreeterProxy.h"
#include "Constants.h"
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



using namespace SDDM;
using namespace std;

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
}

void showUsageHelp(const char*  appName) {
    cout << "Usage: " << appName << " [options] [arguments]\n" 
         << "Options: \n" 
         << "  --theme <theme path>       Set greeter theme\n"
         << "  --socket <socket name>     Set socket name\n" 
         << "  --test                     Testing mode" << endl;
}

int main(int argc, char **argv) {
    
    bool testing = false; 
    QStringList arguments;
    
    for(int ii = 0; ii < argc; ii++) {
        arguments << argv[ii];
    }

    if ( arguments.indexOf("--help") > 0 || arguments.indexOf("-h") > 0 ) {
        showUsageHelp(argv[0]);
        return 1;
    }
    
    if( arguments.indexOf("--test") > 0 ) testing = true; 
    
#ifdef USE_QT5
    // install message handler
    qInstallMessageHandler(SDDM::MessageHandler);

    // create application
    QGuiApplication app(argc, argv);
    // create view
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
#else
    // create application
    QApplication app(argc, argv);
    // create view
    QDeclarativeView view;
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
#endif

    view.engine()->addImportPath(IMPORTS_INSTALL_DIR);

    // create configuration instance
    Configuration configuration(CONFIG_FILE);

    // get socket name
    QString socket = parameter(app.arguments(), "--socket", "");

    // get theme path
    QString theme = parameter(app.arguments(), "--theme", "");

    // read theme metadata
    ThemeMetadata metadata(QString("%1/metadata.desktop").arg(theme));

    // get theme config file
    QString configFile = QString("%1/%2").arg(theme).arg(metadata.configFile());

    // read theme config
    ThemeConfig config(configFile);

    // create models
    SessionModel sessionModel;
    ScreenModel screenModel;
    UserModel userModel;
    GreeterProxy proxy(socket);
    if(!testing && !proxy.isConnected()) {
        qCritical() << "Cannot connect to the daemon - is it running?";
        return EXIT_FAILURE; 
    }
    proxy.setSessionModel(&sessionModel);

    // set context properties
    view.rootContext()->setContextProperty("sessionModel", &sessionModel);
    view.rootContext()->setContextProperty("screenModel", &screenModel);
    view.rootContext()->setContextProperty("userModel", &userModel);
    view.rootContext()->setContextProperty("config", config);
    view.rootContext()->setContextProperty("sddm", &proxy);

    // connect proxy signals
    QObject::connect(&proxy, SIGNAL(loginSucceeded()), &view, SLOT(close()));

    // get theme main script
    QString mainScript = QString("%1/%2").arg(theme).arg(metadata.mainScript());

    // set main script as source
    view.setSource(QUrl::fromLocalFile(mainScript));

    // show view
    view.showFullScreen();
    view.setGeometry(screenModel.geometry());

    // run application
    return app.exec();
}
