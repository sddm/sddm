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

#include <QObject>
#include <QScreen>
#include <QQuickView>
#include <QTimer>

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

    // Event filter shared by all views to update IdleHint in CK/Login1
    class IdleMonitor : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(IdleMonitor)
    public:
        IdleMonitor(QObject *parent);
        ~IdleMonitor();

    public Q_SLOTS:
        void arm(); // Start idle tracking
        void disarm(); // Set IdleHint to false and stop idle tracking

    protected:
        bool eventFilter(QObject *obj, QEvent *ev) override;

    private:
        void idleChanged(bool idle);
        QString m_sessionPath; // DBus path to the session (if available)
        QTimer m_idleTimer; // Reset every time an event happens. On timeout -> idle.
        bool m_isIdle = false;
    };

    class GreeterApp : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(GreeterApp)
    public:
        explicit GreeterApp(QObject *parent = nullptr);

        bool isTestModeEnabled() const;
        void setTestModeEnabled(bool value);

        QString socketName() const;
        void setSocketName(const QString &name);

        QString themePath() const;
        void setThemePath(const QString &path);

    protected:
        void customEvent(QEvent *event) override;

    private slots:
        void addViewForScreen(QScreen *screen);
        void removeViewForScreen(QQuickView *view);

    private:
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
        IdleMonitor *m_idleMonitor { nullptr };

        void startup();
        void activatePrimary();
    };

    class StartupEvent : public QEvent
    {
    public:
        StartupEvent();
    };
}


#endif // GREETERAPP_H
