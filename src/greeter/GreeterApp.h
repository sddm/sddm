/***************************************************************************
* Copyright (c) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Nikita Mikhaylov <nslqqq@gmail.com>
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

#ifndef GREETERAPP_H
#define GREETERAPP_H

#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QQuickView>

class QTranslator;

namespace SDDM {
    class Configuration;
    class ThemeMetadata;
    class ThemeConfig;
    class SessionModel;
    class ScreenModel;
    class UserModel;
    class GreeterProxy;
    class KeyboardModel;


    class GreeterApp : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(GreeterApp)
    public:
        explicit GreeterApp(QObject *parent = nullptr);
        ~GreeterApp() override;

        bool isTestModeEnabled() const;
        void setTestModeEnabled(bool value);

        QString socketName() const;
        void setSocketName(const QString &name);

        QString themePath() const;
        void setThemePath(const QString &path);

    protected:
        void customEvent(QEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private slots:
        void addViewForScreen(QScreen *screen);
        void removeViewForScreen(QQuickView *view);

    private:
        friend class GreeterAppTest;
        bool m_testing = false;
        QString m_socket;
        QString m_themePath;

        QList<QQuickView *> m_views;
        QTranslator *m_theme_translator { nullptr },
                    *m_components_tranlator { nullptr };

        ThemeMetadata *m_metadata { nullptr };
        ThemeConfig *m_themeConfig { nullptr };
        SessionModel *m_sessionModel { nullptr };
        UserModel *m_userModel { nullptr };
        GreeterProxy *m_proxy { nullptr };
        KeyboardModel *m_keyboard { nullptr };
        QPointer<QQuickView> m_controllerView;
        QPointer<QQuickView> m_pendingController;
        QPointer<QQuickItem> m_legacyOverlay;
        bool m_clearPending { false };
        bool m_committedPromotion { false };
        bool m_replacingRoots { false };
        bool m_fallbackStarted { false };
        bool m_embeddedFallback { false };
        bool m_legacyAutoScheduled { false };
        bool m_legacyAutoStarted { false };
        QString m_legacyUser;
        int m_legacySessionIndex { -1 };
        QKeySequence m_retryKey;
        QString m_retryKeyLabel;
        QString m_fallbackErrors;

        void startup();
        void activatePrimary();
        void promoteController();
        void beginFallback(const QString &errors);
        void scheduleLegacyFingerprint();
        void maybeStartLegacyFingerprint();
        void configureRetryKey();
        bool retryLegacyFingerprint();
        void attachLegacyOverlay();
        void destroyLegacyOverlay();
    };

    class StartupEvent : public QEvent
    {
    public:
        StartupEvent();
    };
}


#endif // GREETERAPP_H
