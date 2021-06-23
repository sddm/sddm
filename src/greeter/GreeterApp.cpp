/***************************************************************************
* Copyright (c) 2015-2018 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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
#include "AuthRequest.h"
#include "AuthPrompt.h"
#include "PamTypes.h"

#include "MessageHandler.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>
#include <QTimer>
#include <QTranslator>
#include <QLibraryInfo>
#include <QVersionNumber>
#include <QSurfaceFormat>

#include <iostream>

#define TR(x) QT_TRANSLATE_NOOP("Command line parser", QStringLiteral(x))

static const QEvent::Type StartupEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

namespace SDDM {
    GreeterApp::GreeterApp(QObject *parent)
        : QObject(parent)
    {
        // Translations
        // Components translation
        m_components_tranlator = new QTranslator();
        if (m_components_tranlator->load(QLocale::system(), QString(), QString(), QStringLiteral(COMPONENTS_TRANSLATION_DIR)))
            QCoreApplication::installTranslator(m_components_tranlator);


        // Create models
        m_sessionModel = new SessionModel();
        m_keyboard = new KeyboardModel();
    }

    bool GreeterApp::isTestModeEnabled() const
    {
        return m_testing;
    }

    void GreeterApp::setTestModeEnabled(bool value)
    {
        m_testing = value;
    }

    QString GreeterApp::socketName() const
    {
        return m_socket;
    }

    void GreeterApp::setSocketName(const QString &name)
    {
        m_socket = name;
    }

    QString GreeterApp::themePath() const
    {
        return m_themePath;
    }

    void GreeterApp::setThemePath(const QString &path)
    {
        m_themePath = path;
        if (m_themePath.isEmpty())
            m_themePath = QLatin1String("qrc:/theme");

        // Read theme metadata
        const QString metadataPath = QStringLiteral("%1/metadata.desktop").arg(m_themePath);
        if (m_metadata)
            m_metadata->setTo(metadataPath);
        else
            m_metadata = new ThemeMetadata(metadataPath);

        // Get theme config file
        QString configFile = QStringLiteral("%1/%2").arg(m_themePath).arg(m_metadata->configFile());

        // Read theme config
        if (m_themeConfig)
            m_themeConfig->setTo(configFile);
        else
            m_themeConfig = new ThemeConfig(configFile);

        const bool themeNeedsAllUsers = m_themeConfig->value(QStringLiteral("needsFullUserModel"), true).toBool();
        if(m_userModel && themeNeedsAllUsers && !m_userModel->containsAllUsers()) {
            // The theme needs all users, but the current user model doesn't have them -> recreate
            m_userModel->deleteLater();
            m_userModel = nullptr;
        }

        if (!m_userModel)
            m_userModel = new UserModel(themeNeedsAllUsers, nullptr);

        // Set default icon theme from greeter theme
        if (m_themeConfig->contains(QStringLiteral("iconTheme")))
            QIcon::setThemeName(m_themeConfig->value(QStringLiteral("iconTheme")).toString());

        // Theme specific translation
        if (m_theme_translator)
            m_theme_translator->deleteLater();
        m_theme_translator = new QTranslator();
        if (m_theme_translator->load(QLocale::system(), QString(), QString(),
                           QStringLiteral("%1/%2/").arg(m_themePath, m_metadata->translationsDirectory())))
            QCoreApplication::installTranslator(m_theme_translator);
    }

    void GreeterApp::customEvent(QEvent *event)
    {
        if (event->type() == StartupEventType)
            startup();
    }

    void GreeterApp::addViewForScreen(QScreen *screen) {
        // create view
        QQuickView *view = new QQuickView();
        view->setScreen(screen);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        //view->setGeometry(QRect(QPoint(0, 0), screen->geometry().size()));
        view->setGeometry(screen->geometry());
        view->setFlags(Qt::FramelessWindowHint);
        m_views.append(view);

        // remove the view when the screen is removed, but we
        // need to be careful here since Qt will move the view to
        // another screen before this signal is emitted so we
        // pass a pointer to the view to our slot
        connect(qGuiApp, &QGuiApplication::screenRemoved, view, [view, this, screen](QScreen *s) {
            if (s == screen)
                removeViewForScreen(view);
        });

        // always resize when the screen geometry changes
        connect(screen, &QScreen::geometryChanged, this, [view](const QRect &r) {
            view->setGeometry(r);
        });

        view->engine()->addImportPath(QStringLiteral(IMPORTS_INSTALL_DIR));

        // connect proxy signals
        connect(m_proxy, &GreeterProxy::loginSucceeded, view, &QQuickView::close);

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
        view->rootContext()->setContextProperty(QStringLiteral("request"), m_proxy->getRequest());
        view->rootContext()->setContextProperty(QStringLiteral("keyboard"), m_keyboard);
        view->rootContext()->setContextProperty(QStringLiteral("primaryScreen"), QGuiApplication::primaryScreen() == screen);
        view->rootContext()->setContextProperty(QStringLiteral("__sddm_errors"), QString());

        // provide pam result defines from _pam_types.h to theme
        qmlRegisterUncreatableType<PamTypes>("PamTypes", 1, 0, "PamTypes", QStringLiteral("PamTypes object creation not allowed"));

        // register AuthRequest and AuthPrompt type for pamRequest signal
        qmlRegisterUncreatableType<AuthRequest>("SddmAuth", 1, 0, "AuthRequest", QStringLiteral("AuthRequest object creation not allowed"));
        qmlRegisterUncreatableType<AuthPrompt>("SddmAuth", 1, 0, "AuthPrompt", QStringLiteral("AuthPrompt object creation not allowed"));

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
            const auto errorList = view->errors();
            for(const QQmlError &e : errorList) {
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

    void GreeterApp::startup()
    {
        // Connect to the daemon
        m_proxy = new GreeterProxy(m_socket);
        if (!m_testing && !m_proxy->isConnected()) {
            qCritical() << "Cannot connect to the daemon - is it running?";
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }

        // Set numlock upon start
        if (m_keyboard->enabled()) {
            if (mainConfig.Numlock.get() == MainConfig::NUM_SET_ON)
                m_keyboard->setNumLockState(true);
            else if (mainConfig.Numlock.get() == MainConfig::NUM_SET_OFF)
                m_keyboard->setNumLockState(false);
        }

        // Set font
        const QString fontStr = mainConfig.Theme.Font.get();
        if (!fontStr.isEmpty()) {
            QFont font;
            if (font.fromString(fontStr)) {
                QGuiApplication::setFont(font);
            }
        }

        // Set session model on proxy
        m_proxy->setSessionModel(m_sessionModel);

        // Create views
        const QList<QScreen *> screens = qGuiApp->primaryScreen()->virtualSiblings();
        for (QScreen *screen : screens)
            addViewForScreen(screen);

        // Handle screens
        connect(qGuiApp, &QGuiApplication::screenAdded, this, &GreeterApp::addViewForScreen);
        connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *) {
            activatePrimary();
        });
    }

    void GreeterApp::activatePrimary() {
        // activate and give focus to the window assigned to the primary screen
        for (QQuickView *view : qAsConst(m_views)) {
            if (view->screen() == QGuiApplication::primaryScreen()) {
                view->requestActivate();
                break;
            }
        }
    }

    StartupEvent::StartupEvent()
        : QEvent(StartupEventType)
    {
    }
}

int main(int argc, char **argv)
{
    // Install message handler
    qInstallMessageHandler(SDDM::GreeterMessageHandler);

    // We set an attribute based on the platform we run on.
    // We only know the platform after we constructed QGuiApplication
    // though, so we need to find it out ourselves.
    QString platform;
    for (int i = 1; i < argc - 1; ++i) {
        if(qstrcmp(argv[i], "-platform") == 0) {
            platform = QString::fromUtf8(argv[i + 1]);
        }
    }
    if (platform.isEmpty()) {
        platform = QString::fromUtf8(qgetenv("QT_QPA_PLATFORM"));
    }
    if (platform.isEmpty()) {
        platform = QStringLiteral("xcb");
    }

    // HiDPI
    bool hiDpiEnabled = false;
    if (platform == QStringLiteral("xcb"))
        hiDpiEnabled = SDDM::mainConfig.X11.EnableHiDPI.get();
    else if (platform.startsWith(QStringLiteral("wayland")))
        hiDpiEnabled = SDDM::mainConfig.Wayland.EnableHiDPI.get();
    if (hiDpiEnabled) {
        qDebug() << "High-DPI autoscaling Enabled";
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    } else {
        qDebug() << "High-DPI autoscaling not Enabled";
    }

    if (QLibraryInfo::version() >= QVersionNumber(5, 13, 0)) {
        auto format(QSurfaceFormat::defaultFormat());
        format.setOption(QSurfaceFormat::ResetNotification);
        QSurfaceFormat::setDefaultFormat(format);
    }

    // Some themes may use KDE components and that will automatically load KDE's
    // crash handler which we don't want counterintuitively setting this env
    // disables that handler
    qputenv("KDE_DEBUG", "1");

    // Qt IM module
    if (!SDDM::mainConfig.InputMethod.get().isEmpty())
        qputenv("QT_IM_MODULE", SDDM::mainConfig.InputMethod.get().toLocal8Bit().constData());

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(TR("SDDM greeter"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption testModeOption(QLatin1String("test-mode"), TR("Start greeter in test mode"));
    parser.addOption(testModeOption);

    QCommandLineOption socketOption(QLatin1String("socket"), TR("Socket name"), TR("name"));
    parser.addOption(socketOption);

    QCommandLineOption themeOption(QLatin1String("theme"), TR("Greeter theme"), TR("path"));
    parser.addOption(themeOption);

    parser.process(app);

    SDDM::GreeterApp *greeter = new SDDM::GreeterApp();
    greeter->setTestModeEnabled(parser.isSet(testModeOption));
    greeter->setSocketName(parser.value(socketOption));
    greeter->setThemePath(parser.value(themeOption));
    QCoreApplication::postEvent(greeter, new SDDM::StartupEvent());

    return app.exec();
}
