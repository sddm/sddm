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
#include "SignalHandler.h"
#include "ThemeConfig.h"
#include "ThemeMetadata.h"
#include "UserModel.h"
#include "KeyboardModel.h"

#include "MessageHandler.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlComponent>
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
        m_components_tranlator = new QTranslator(this);
        if (m_components_tranlator->load(QLocale::system(), QString(), QString(), QStringLiteral(COMPONENTS_TRANSLATION_DIR)))
            QCoreApplication::installTranslator(m_components_tranlator);


        // Create models
        m_sessionModel = new SessionModel(this);
        m_keyboard = new KeyboardModel();
    }

    GreeterApp::~GreeterApp() {
        m_replacingRoots = true;
        m_controllerView.clear();
        m_pendingController.clear();
        qDeleteAll(m_views);
        m_views.clear();
        delete m_keyboard;
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
            m_metadata = new ThemeMetadata(metadataPath, this);

        // Get theme config file
        QString configFile = QStringLiteral("%1/%2").arg(m_themePath).arg(m_metadata->configFile());

        // Read theme config
        if (m_themeConfig)
            m_themeConfig->setTo(configFile);
        else
            m_themeConfig = new ThemeConfig(configFile, this);

        const bool themeNeedsAllUsers = m_themeConfig->value(QStringLiteral("needsFullUserModel"), true).toBool();
        if(m_userModel && themeNeedsAllUsers && !m_userModel->containsAllUsers()) {
            // The theme needs all users, but the current user model doesn't have them -> recreate
            m_userModel->deleteLater();
            m_userModel = nullptr;
        }

        if (!m_userModel)
            m_userModel = new UserModel(themeNeedsAllUsers, this);

        // Set default icon theme from greeter theme
        if (m_themeConfig->contains(QStringLiteral("iconTheme")))
            QIcon::setThemeName(m_themeConfig->value(QStringLiteral("iconTheme")).toString());

        // Theme specific translation
        if (m_theme_translator)
            m_theme_translator->deleteLater();
        m_theme_translator = new QTranslator(this);
        if (m_theme_translator->load(QLocale::system(), QString(), QString(),
                           QStringLiteral("%1/%2/").arg(m_themePath, m_metadata->translationsDirectory())))
            QCoreApplication::installTranslator(m_theme_translator);
    }

    void GreeterApp::customEvent(QEvent *event)
    {
        if (event->type() == StartupEventType)
            startup();
    }

    bool GreeterApp::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == m_controllerView && event->type() == QEvent::KeyPress && !m_retryKey.isEmpty()) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if (!key->isAutoRepeat()) {
                const int mods = int(key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
                if (QKeySequence(key->key() | mods).matches(m_retryKey) == QKeySequence::ExactMatch) {
                    if (retryLegacyFingerprint())
                        return true;
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

    void GreeterApp::addViewForScreen(QScreen *screen) {
        if (!screen || m_replacingRoots) return;
        QQuickView *view = new QQuickView();
        view->setScreen(screen);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setGeometry(screen->geometry());
        view->setFlags(Qt::FramelessWindowHint);
        m_views.append(view);

        connect(qGuiApp, &QGuiApplication::screenRemoved, view, [this, view, screen](QScreen *removed) {
            if (removed == screen) removeViewForScreen(view);
        });
        connect(screen, &QScreen::geometryChanged, view, [view](const QRect &geometry) {
            view->setGeometry(geometry);
        });
        connect(m_proxy, &GreeterProxy::loginSucceeded, view, &QQuickView::close);
        view->engine()->addImportPath(QStringLiteral(IMPORTS_INSTALL_DIR));
        ScreenModel *screenModel = new ScreenModel(screen, view);

        const bool controller = !m_clearPending && !m_controllerView
            && (screen == QGuiApplication::primaryScreen() || m_views.size() == 1);
        if (controller) {
            m_controllerView = view;
            m_proxy->setAuthenticationController(true);
        }
        if (!m_pendingController || (m_clearPending && screen == QGuiApplication::primaryScreen()))
            m_pendingController = view;
        view->rootContext()->setContextProperty(QStringLiteral("sessionModel"), m_sessionModel);
        view->rootContext()->setContextProperty(QStringLiteral("screenModel"), screenModel);
        view->rootContext()->setContextProperty(QStringLiteral("userModel"), m_userModel);
        view->rootContext()->setContextProperty(QStringLiteral("config"), m_themeConfig);
        view->rootContext()->setContextProperty(QStringLiteral("sddm"), m_proxy);
        view->rootContext()->setContextProperty(QStringLiteral("keyboard"), m_keyboard);
        view->rootContext()->setContextProperty(QStringLiteral("authenticationController"), controller);
        view->rootContext()->setContextProperty(QStringLiteral("primaryScreen"), screen == QGuiApplication::primaryScreen());
        view->rootContext()->setContextProperty(QStringLiteral("__sddm_errors"), m_fallbackErrors);

        connect(view, &QQuickView::statusChanged, this, [this, view](QQuickView::Status status) {
            if (status != QQuickView::Error) return;
            QString errors;
            for (const QQmlError &error : view->errors()) {
                qWarning() << error;
                errors += QLatin1String("\n") + error.toString();
            }
            if (m_embeddedFallback) {
                qCritical() << "Embedded greeter theme failed to load";
                QCoreApplication::exit(EXIT_FAILURE);
            } else if (!m_fallbackStarted) {
                beginFallback(errors);
            }
        });
        const QString mainScript = QStringLiteral("%1/%2").arg(m_themePath, m_metadata->mainScript());
        const QUrl mainScriptUrl = m_themePath.startsWith(QLatin1String("qrc:/"))
            ? QUrl(mainScript) : QUrl::fromLocalFile(mainScript);
        qInfo("Loading %s...", qPrintable(mainScriptUrl.toString()));
        view->setSource(mainScriptUrl);
        if (QQuickItem *root = view->rootObject()) {
            root->setCursor(QCursor(Qt::ArrowCursor));
            connect(root, &QObject::destroyed, this, [this, view] {
                if (m_replacingRoots || m_controllerView != view) return;
                // A mirror has no lifecycle authority. A lost controlling root
                // cannot leave invisible automatic authentication behind.
                removeViewForScreen(view);
            });
        }
        if (view->status() != QQuickView::Ready) return;
        view->showFullScreen();
        if (controller) {
            view->requestActivate();
            scheduleLegacyFingerprint();
        }
        if (screen == QGuiApplication::primaryScreen() && m_controllerView != view)
            activatePrimary();
    }

    void GreeterApp::removeViewForScreen(QQuickView *view) {
        if (!m_views.removeOne(view)) return;
        const bool wasController = m_controllerView == view;
        if (m_pendingController == view) m_pendingController.clear();
        if (wasController) {
            destroyLegacyOverlay();
            m_controllerView.clear();
            m_proxy->setAuthenticationController(false);
            m_clearPending = true;
        }
        view->deleteLater();
        if (wasController) {
            activatePrimary();
            m_proxy->clearAuthenticationContext();
        }
    }

    void GreeterApp::beginFallback(const QString &errors) {
        qWarning() << "Fallback to embedded theme";
        destroyLegacyOverlay();
        m_fallbackStarted = true;
        m_fallbackErrors = errors;
        m_clearPending = true;
        m_proxy->setAuthenticationController(false);
        for (QQuickView *view : qAsConst(m_views))
            view->rootContext()->setContextProperty(QStringLiteral("authenticationController"), false);
        m_proxy->disableAuthenticationMethods();
        // Destroying a root from its own load-status callback is unsafe. The
        // method prohibition and clear request above take effect immediately.
        QTimer::singleShot(0, this, [this] {
            m_replacingRoots = true;
            m_controllerView.clear();
            m_pendingController.clear();
            const auto oldViews = m_views;
            m_views.clear();
            qDeleteAll(oldViews);
            setThemePath(QStringLiteral("qrc:/theme"));
            m_embeddedFallback = true;
            m_replacingRoots = false;
            QScreen *primary = QGuiApplication::primaryScreen();
            if (primary) addViewForScreen(primary);
            for (QScreen *screen : QGuiApplication::screens())
                if (screen != primary) addViewForScreen(screen);
            if (m_committedPromotion) promoteController();
            else activatePrimary();
        });
    }

    void GreeterApp::startup()
    {
        m_proxy = new GreeterProxy(m_socket, this);
        if (!m_testing && !m_proxy->isConnected()) {
            qCritical() << "Cannot connect to the daemon - is it running?";
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }
        if (m_keyboard->enabled()) {
            if (mainConfig.Numlock.get() == MainConfig::NUM_SET_ON)
                m_keyboard->setNumLockState(true);
            else if (mainConfig.Numlock.get() == MainConfig::NUM_SET_OFF)
                m_keyboard->setNumLockState(false);
        }
        const QString fontStr = mainConfig.Theme.Font.get();
        if (!fontStr.isEmpty()) {
            QFont font;
            if (font.fromString(fontStr))
                QGuiApplication::setFont(font);
        }
        m_proxy->setSessionModel(m_sessionModel);
        connect(m_proxy, &GreeterProxy::socketDisconnected, qGuiApp, &QCoreApplication::quit);
        connect(m_proxy, &GreeterProxy::authenticationMethodsAvailableChanged, this, [this](bool available) {
            if (available) scheduleLegacyFingerprint();
        });
        connect(m_proxy, &GreeterProxy::authenticationContextCleared, this, [this](quint64) {
            m_clearPending = false;
            m_committedPromotion = false;
            if (!m_replacingRoots && (!m_fallbackStarted || m_embeddedFallback))
                promoteController();
        });
        connect(m_proxy, &GreeterProxy::authenticationContextRejected, this, [this](quint64, const QString &) {
            m_committedPromotion = true;
            if (!m_replacingRoots && (!m_fallbackStarted || m_embeddedFallback))
                promoteController();
        });
        m_embeddedFallback = m_themePath.startsWith(QLatin1String("qrc:/"));
        if (m_embeddedFallback) m_proxy->disableAuthenticationMethods();
        QScreen *primary = QGuiApplication::primaryScreen();
        if (primary) addViewForScreen(primary);
        for (QScreen *screen : QGuiApplication::screens())
            if (screen != primary) addViewForScreen(screen);
        connect(qGuiApp, &QGuiApplication::screenAdded, this, &GreeterApp::addViewForScreen);
        connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *) { activatePrimary(); });
    }

    void GreeterApp::activatePrimary() {
        QQuickView *next = nullptr;
        for (QQuickView *view : std::as_const(m_views)) {
            if (view->screen() == QGuiApplication::primaryScreen()) {
                next = view;
                break;
            }
        }
        if (!next && !m_views.isEmpty()) next = m_views.first();
        if (m_controllerView == next && !m_clearPending) return;
        m_pendingController = next;
        if (m_clearPending) {
            if (m_committedPromotion) promoteController();
            return;
        }
        m_clearPending = true;
        destroyLegacyOverlay();
        m_controllerView.clear();
        m_proxy->setAuthenticationController(false);
        for (QQuickView *view : qAsConst(m_views))
            view->rootContext()->setContextProperty(QStringLiteral("authenticationController"), false);
        m_proxy->clearAuthenticationContext();
    }

    void GreeterApp::promoteController() {
        if (!m_pendingController && !m_views.isEmpty())
            m_pendingController = m_views.first();
        m_controllerView = m_pendingController;
        m_proxy->setAuthenticationController(bool(m_controllerView));
        for (QQuickView *view : qAsConst(m_views)) {
            const bool controller = view == m_controllerView;
            view->rootContext()->setContextProperty(QStringLiteral("authenticationController"), controller);
            if (controller) view->requestActivate();
        }
        if (m_controllerView && !m_proxy->hasAuthenticationActivity()) {
            m_legacyAutoStarted = false;
            scheduleLegacyFingerprint();
        }
    }

    void GreeterApp::scheduleLegacyFingerprint() {
        if (m_legacyAutoScheduled || m_legacyAutoStarted || m_embeddedFallback || m_fallbackStarted)
            return;
        if (!mainConfig.AutoFingerprintForLegacyThemes.get())
            return;
        m_legacyAutoScheduled = true;
        // Defer briefly so themes implementing the fingerprint API can opt in first.
        QTimer::singleShot(100, this, [this] {
            m_legacyAutoScheduled = false;
            maybeStartLegacyFingerprint();
        });
    }

    void GreeterApp::maybeStartLegacyFingerprint() {
        if (m_legacyAutoStarted || m_embeddedFallback || m_fallbackStarted || m_clearPending)
            return;
        if (!mainConfig.AutoFingerprintForLegacyThemes.get())
            return;
        if (!m_controllerView || m_controllerView->status() != QQuickView::Ready)
            return;
        if (!m_proxy || !m_proxy->authenticationController()
            || !m_proxy->authenticationMethodsAvailable()
            || m_proxy->authenticationBusy()
            || m_proxy->hasAuthenticationActivity())
            return;

        QString user = m_userModel ? m_userModel->lastUser() : QString();
        if (user.isEmpty() && m_userModel && m_userModel->rowCount() > 0) {
            const int idx = (m_userModel->lastIndex() >= 0 && m_userModel->lastIndex() < m_userModel->rowCount())
                ? m_userModel->lastIndex() : 0;
            user = m_userModel->data(m_userModel->index(idx, 0), UserModel::NameRole).toString();
        }
        int sessionIndex = m_sessionModel ? m_sessionModel->lastIndex() : -1;
        if (m_sessionModel && (sessionIndex < 0 || sessionIndex >= m_sessionModel->rowCount())
            && m_sessionModel->rowCount() > 0)
            sessionIndex = 0;
        if (user.isEmpty() || sessionIndex < 0)
            return;

        m_legacyAutoStarted = true;
        m_legacyUser = user;
        m_legacySessionIndex = sessionIndex;
        configureRetryKey();
        m_controllerView->installEventFilter(this);
        if (mainConfig.FingerprintStatusOverlay.get())
            attachLegacyOverlay();
        m_proxy->startFingerprint(user, sessionIndex);
    }

    void GreeterApp::configureRetryKey() {
        const QString raw = mainConfig.FingerprintRetryKey.get().trimmed();
        if (raw.isEmpty() || raw.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
            m_retryKey = QKeySequence();
            m_retryKeyLabel.clear();
            return;
        }
        const QKeySequence seq = QKeySequence::fromString(raw, QKeySequence::PortableText);
        if (seq.count() == 1) {
            m_retryKey = seq;
            m_retryKeyLabel = seq.toString(QKeySequence::PortableText);
        } else {
            m_retryKey = QKeySequence(Qt::Key_F4);
            m_retryKeyLabel = QStringLiteral("F4");
        }
    }

    bool GreeterApp::retryLegacyFingerprint() {
        if (!m_legacyAutoStarted || m_embeddedFallback || m_fallbackStarted || m_clearPending)
            return false;
        if (!m_controllerView || !m_proxy || !m_proxy->authenticationController()
            || !m_proxy->authenticationMethodsAvailable() || m_proxy->authenticationBusy())
            return false;
        if (m_legacyUser.isEmpty() || m_legacySessionIndex < 0)
            return false;
        const QString st = m_proxy->fingerprintState();
        if (st != QLatin1String("failed") && st != QLatin1String("timed_out")
            && st != QLatin1String("cancelled") && st != QLatin1String("ready"))
            return false;
        m_proxy->startFingerprint(m_legacyUser, m_legacySessionIndex);
        return true;
    }

    void GreeterApp::attachLegacyOverlay() {
        if (m_legacyOverlay || !m_controllerView || m_embeddedFallback || m_fallbackStarted)
            return;
        QQuickItem *root = m_controllerView->rootObject();
        if (!root)
            return;
        static const QByteArray overlayQml = QByteArrayLiteral(R"QML(
import QtQuick 2.15
Rectangle {
    id: fpOverlay
    objectName: "fploginLegacyStatusOverlay"
    property string retryKeyLabel: "F4"
    readonly property string retryHint: retryKeyLabel.length > 0 ? ("press " + retryKeyLabel + " to retry or use password") : "use password"
    readonly property string st: sddm ? sddm.fingerprintState : ""
    readonly property string labelText: {
        if (st === "scanning") return "Fingerprint ready — touch sensor"
        if (st === "retry") return "Fingerprint not recognized — try again"
        if (st === "failed") return "Fingerprint failed — " + retryHint
        if (st === "timed_out") return "Fingerprint timed out — " + retryHint
        if (st === "matched") return "Fingerprint verified"
        return ""
    }
    readonly property color dotColor: {
        if (st === "scanning") return "#38bdf8"
        if (st === "retry") return "#f59e0b"
        if (st === "failed" || st === "timed_out") return "#ef4444"
        if (st === "matched") return "#22c55e"
        return "#94a3b8"
    }
    visible: authenticationController && labelText.length > 0
    focus: false
    activeFocusOnTab: false
    z: 999999
    radius: 16
    color: "#cc111318"
    border.color: "#33ffffff"
    border.width: 1
    implicitWidth: row.implicitWidth + 28
    implicitHeight: row.implicitHeight + 14
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 28
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: fpOverlay.dotColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            objectName: "fploginLegacyStatusText"
            text: fpOverlay.labelText
            color: "#f1f5f9"
            font.pixelSize: 13
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
)QML");
        QQmlComponent component(m_controllerView->engine());
        component.setData(overlayQml, QUrl(QStringLiteral("qrc:/fplogin-legacy-overlay.qml")));
        if (component.status() != QQmlComponent::Ready) {
            qWarning() << "Failed to create legacy fingerprint overlay:" << component.errorString();
            return;
        }
        QObject *obj = component.create(m_controllerView->rootContext());
        if (auto *item = qobject_cast<QQuickItem *>(obj)) {
            item->setProperty("retryKeyLabel", m_retryKeyLabel);
            item->setParentItem(root);
            item->setParent(root);
            m_legacyOverlay = item;
        } else {
            delete obj;
        }
    }

    void GreeterApp::destroyLegacyOverlay() {
        if (m_legacyOverlay) {
            m_legacyOverlay->deleteLater();
            m_legacyOverlay.clear();
        }
    }

    StartupEvent::StartupEvent()
        : QEvent(StartupEventType)
    {
    }
}

#ifndef SDDM_GREETER_APP_TEST
int main(int argc, char **argv)
{
    bool testMode = false;
    // We set an attribute based on the platform we run on.
    // We only know the platform after we constructed QGuiApplication
    // though, so we need to find it out ourselves.
    QString platform;
    for (int i = 1; i < argc - 1; ++i) {
        if(qstrcmp(argv[i], "-platform") == 0) {
            platform = QString::fromUtf8(argv[i + 1]);
        }
        testMode |= qstrcmp(argv[i], "--test-mode") == 0;
    }
    if (platform.isEmpty()) {
        platform = QString::fromUtf8(qgetenv("QT_QPA_PLATFORM"));
    }
    if (platform.isEmpty()) {
        platform = qEnvironmentVariableIsSet("WAYLAND_DISPLAY") ? QStringLiteral("wayland") : QStringLiteral("xcb");
    }

    // Install message handler
    if (!testMode)
        qInstallMessageHandler(SDDM::GreeterMessageHandler);

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
    QString inputMethod = SDDM::mainConfig.InputMethod.get();
    // Using qtvirtualkeyboard as IM on wayland doesn't really work,
    // it has to be done by the compositor instead.
    if (platform.startsWith(QStringLiteral("wayland")) && inputMethod == QStringLiteral("qtvirtualkeyboard"))
        inputMethod = QString{};

    if (!inputMethod.isEmpty())
        qputenv("QT_IM_MODULE", inputMethod.toLocal8Bit());

    QGuiApplication app(argc, argv);

    // Don't quit when the last screen and thus the last view disappears
    app.setQuitOnLastWindowClosed(false);

    SDDM::SignalHandler s;
    QObject::connect(&s, &SDDM::SignalHandler::sigtermReceived, &app, [] {
        QCoreApplication::instance()->exit(-1);
    });
    QObject::connect(&s, &SDDM::SignalHandler::sigintReceived, &app, [] {
        QCoreApplication::instance()->exit(-1);
    });

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
#endif
