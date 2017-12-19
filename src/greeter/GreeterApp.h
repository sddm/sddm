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

#include <QGuiApplication>
#include <QScreen>
#include <QQuickView>
#include <QSortFilterProxyModel>

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


    class GreeterApp : public QGuiApplication
    {
        Q_OBJECT
        Q_DISABLE_COPY(GreeterApp)
    public:
        explicit GreeterApp(int &argc, char **argv);

        static GreeterApp *instance() { return self; }

    private slots:
        void addViewForScreen(QScreen *screen);
        void removeViewForScreen(QQuickView *view);

    private:
        static GreeterApp *self;

        QList<QQuickView *> m_views;
        QTranslator *m_theme_translator { nullptr },
                    *m_components_tranlator { nullptr };

        QString m_themePath;
        ThemeMetadata *m_metadata { nullptr };
        ThemeConfig *m_themeConfig { nullptr };
        SessionModel *m_sessionModel { nullptr };
        UserModel *m_userModel { nullptr };
        GreeterProxy *m_proxy { nullptr };
        KeyboardModel *m_keyboard { nullptr };
        QSortFilterProxyModel *m_sort_filterModel {nullptr};

        void activatePrimary();
    };
}


#endif // GREETERAPP_H
