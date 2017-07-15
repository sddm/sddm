/***************************************************************************
* Copyright (c) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "GreeterApp.h"
#include "Configuration.h"
#include "GreeterProxy.h"
#include "Constants.h"
#include "ScreenModel.h"
#include "SessionModel.h"
#include "ThemeConfig.h"
#include "ThemeMetadata.h"
#include "UserModel.h"
#include "KeyboardModel.h"

#include "MessageHandler.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>
#include <QTimer>
#include <QTranslator>

#include <iostream>

namespace SDDM {
    QString parameter(const QStringList &arguments, const QString &key, const QString &defaultValue) {
        int index = arguments.indexOf(key);

        if ((index < 0) || (index >= arguments.size() - 1))
            return defaultValue;

        QString value = arguments.at(index + 1);

        if (value.startsWith(QLatin1Char('-')))
            return defaultValue;

        return value;
    }

    GreeterApp *GreeterApp::self = nullptr;

    GreeterApp::GreeterApp(int &argc, char **argv) : QGuiApplication(argc, argv) {
        // point instance to this
        self = this;

        // Parse arguments
        bool testing = false;

        if (arguments().contains(QStringLiteral("--test-mode")))
            testing = true;

        // get socket name
        QString socket = parameter(arguments(), QStringLiteral("--socket"), QString());

        // get theme path (fallback to internal theme)
        m_themePath = parameter(arguments(), QStringLiteral("--theme"), QString());
        if (m_themePath.isEmpty())
            m_themePath = QLatin1String("qrc:/theme");

        // read theme metadata
        m_metadata = new ThemeMetadata(QStringLiteral("%1/metadata.desktop").arg(m_themePath));

        // Translations
        // Components translation
        m_components_tranlator = new QTranslator();
        if (m_components_tranlator->load(QLocale::system(), QString(), QString(), QStringLiteral(COMPONENTS_TRANSLATION_DIR)))
            installTranslator(m_components_tranlator);

        // Theme specific translation
        m_theme_translator = new QTranslator();
        if (m_theme_translator->load(QLocale::system(), QString(), QString(),
                           QStringLiteral("%1/%2/").arg(m_themePath, m_metadata->translationsDirectory())))
            installTranslator(m_theme_translator);

        // get theme config file
        QString configFile = QStringLiteral("%1/%2").arg(m_themePath).arg(m_metadata->configFile());

        // read theme config
        m_themeConfig = new ThemeConfig(configFile);

        // set default icon theme from greeter theme
        if (m_themeConfig->contains(QStringLiteral("iconTheme")))
            QIcon::setThemeName(m_themeConfig->value(QStringLiteral("iconTheme")).toString());

        // create models

        m_sessionModel = new SessionModel();
        m_userModel = new UserModel();
        m_proxy = new GreeterProxy(socket);
        m_keyboard = new KeyboardModel();

        if(!testing && !m_proxy->isConnected()) {
            qCritical() << "Cannot connect to the daemon - is it running?";
            exit(EXIT_FAILURE);
        }

        // Set numlock upon start
        if (m_keyboard->enabled()) {
            if (mainConfig.Numlock.get() == MainConfig::NUM_SET_ON)
                m_keyboard->setNumLockState(true);
            else if (mainConfig.Numlock.get() == MainConfig::NUM_SET_OFF)
                m_keyboard->setNumLockState(false);
        }

        m_proxy->setSessionModel(m_sessionModel);

        // create views
        QList<QScreen *> screens = primaryScreen()->virtualSiblings();
        Q_FOREACH (QScreen *screen, screens)
            addViewForScreen(screen);

        // handle screens
        connect(this, &GreeterApp::screenAdded, this, &GreeterApp::addViewForScreen);
        connect(this, &GreeterApp::primaryScreenChanged, this, [this](QScreen *) {
            activatePrimary();
        });
    }

    void GreeterApp::addViewForScreen(QScreen *screen) {
        // create view
        QQuickView *view = new QQuickView();
        view->setScreen(screen);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        //view->setGeometry(QRect(QPoint(0, 0), screen->geometry().size()));
        view->setGeometry(screen->geometry());
        m_views.append(view);

        // remove the view when the screen is removed, but we
        // need to be careful here since Qt will move the view to
        // another screen before this signal is emitted so we
        // pass a pointer to the view to our slot
        connect(this, &GreeterApp::screenRemoved, this, [view, this](QScreen *) {
            removeViewForScreen(view);
        });

        // always resize when the screen geometry changes
        connect(screen, &QScreen::geometryChanged, this, [view](const QRect &r) {
            view->setGeometry(r);
        });

        view->engine()->addImportPath(QStringLiteral(IMPORTS_INSTALL_DIR));

        // connect proxy signals
        connect(m_proxy, SIGNAL(loginSucceeded()), view, SLOT(close()));

        // we used to have only one window as big as the virtual desktop,
        // QML took care of creating an item for each screen by iterating on
        // the screen model. However we now have a better approach: we create
        // a view for each screen that compose the virtual desktop and thus
        // the QML code for each screen is responsible for drawing only its
        // screen. By doing so we actually make the screen model useless, but
        // we want to keep it for compatibility reasons, we do however create
        // one for each view and expose only the screen that the view belongs to
        // in order to avoid creating items with different sizes.
        ScreenModel *screenModel = new ScreenModel(screen, view);

        // set context properties
        view->rootContext()->setContextProperty(QStringLiteral("sessionModel"), m_sessionModel);
        view->rootContext()->setContextProperty(QStringLiteral("screenModel"), screenModel);
        view->rootContext()->setContextProperty(QStringLiteral("userModel"), m_userModel);
        view->rootContext()->setContextProperty(QStringLiteral("config"), *m_themeConfig);
        view->rootContext()->setContextProperty(QStringLiteral("sddm"), m_proxy);
        view->rootContext()->setContextProperty(QStringLiteral("keyboard"), m_keyboard);
        view->rootContext()->setContextProperty(QStringLiteral("primaryScreen"), QGuiApplication::primaryScreen() == screen);
        view->rootContext()->setContextProperty(QStringLiteral("__sddm_errors"), QString());

        // get theme main script
        QString mainScript = QStringLiteral("%1/%2").arg(m_themePath).arg(m_metadata->mainScript());
        QUrl mainScriptUrl;
        if (m_themePath.startsWith(QLatin1String("qrc:/")))
            mainScriptUrl = QUrl(mainScript);
        else
            mainScriptUrl = QUrl::fromLocalFile(mainScript);

        // load theme from resources when an error has occurred
        connect(view, &QQuickView::statusChanged, this, [view](QQuickView::Status status) {
            if (status != QQuickView::Error)
                return;

            QString errors;
            Q_FOREACH(const QQmlError &e, view->errors()) {
                qWarning() << e;
                errors += QLatin1String("\n") + e.toString();
            }

            qWarning() << "Fallback to embedded theme";
            view->rootContext()->setContextProperty(QStringLiteral("__sddm_errors"), errors);
            view->setSource(QUrl(QStringLiteral("qrc:/theme/Main.qml")));
        });

        // set main script as source
        qInfo("Loading %s...", qPrintable(mainScriptUrl.toString()));
        view->setSource(mainScriptUrl);

        // set default cursor
        QCursor cursor(Qt::ArrowCursor);
        view->rootObject()->setCursor(cursor);

        // show
        qDebug() << "Adding view for" << screen->name() << screen->geometry();
        view->show();

        // activate windows for the primary screen to give focus to text fields
        if (QGuiApplication::primaryScreen() == screen)
            view->requestActivate();
    }

    void GreeterApp::removeViewForScreen(QQuickView *view) {
        // screen is gone, remove the window
        m_views.removeOne(view);
        view->deleteLater();
    }

    void GreeterApp::activatePrimary() {
        // activate and give focus to the window assigned to the primary screen
        Q_FOREACH (QQuickView *view, m_views) {
            if (view->screen() == QGuiApplication::primaryScreen()) {
                view->requestActivate();
                break;
            }
        }
    }
}

int main(int argc, char **argv) {
    // install message handler
    qInstallMessageHandler(SDDM::GreeterMessageHandler);

    // HiDPI
    bool hiDpiEnabled = false;
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
        hiDpiEnabled = SDDM::mainConfig.X11.EnableHiDPI.get();
    else if (QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
        hiDpiEnabled = SDDM::mainConfig.Wayland.EnableHiDPI.get();
    if (hiDpiEnabled) {
        qDebug() << "High-DPI autoscaling Enabled";
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    } else {
        qDebug() << "High-DPI autoscaling not Enabled";
    }

    QStringList arguments;

    for (int i = 0; i < argc; i++)
        arguments << QString::fromLocal8Bit(argv[i]);

    if (arguments.contains(QStringLiteral("--help")) || arguments.contains(QStringLiteral("-h"))) {
        std::cout << "Usage: " << argv[0] << " [options] [arguments]\n"
                     "Options: \n"
                     "  --theme <theme path>       Set greeter theme\n"
                     "  --socket <socket name>     Set socket name\n"
                     "  --test-mode                Start greeter in test mode" << std::endl;

        return EXIT_FAILURE;
    }

    SDDM::GreeterApp app(argc, argv);

    return app.exec();
}
