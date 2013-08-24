/***************************************************************************
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

#ifdef USE_QT5
#include <QGuiApplication>
#include <QQuickView>
#else
#include <QApplication>
#include <QDeclarativeView>
#endif

class QTranslator;

namespace SDDM {
    class Configuration;
    class ThemeMetadata;
    class ThemeConfig;
    class SessionModel;
    class ScreenModel;
    class ScreenModel;
    class UserModel;
    class GreeterProxy;
    class KeyboardModel;


    class GreeterApp : public
#ifdef USE_QT5
    QGuiApplication
#else
    QApplication
#endif
    {
        Q_OBJECT
        Q_DISABLE_COPY(GreeterApp)
    public:
        explicit GreeterApp(int argc, char **argv);

        static GreeterApp *instance() { return self; }

    private slots:
        void show();

    private:
        static GreeterApp *self;

#ifdef USE_QT5
        QQuickView *m_view { nullptr };
#else
        QDeclarativeView *m_view { nullptr };
#endif
        QTranslator *m_theme_translator { nullptr },
                    *m_components_tranlator { nullptr };

        Configuration *m_configuration { nullptr };
        ThemeMetadata *m_metadata { nullptr };
        ThemeConfig *m_themeConfig { nullptr };
        SessionModel *m_sessionModel { nullptr };
        ScreenModel *m_screenModel { nullptr  };
        UserModel *m_userModel { nullptr };
        GreeterProxy *m_proxy { nullptr };
        KeyboardModel *m_keyboard { nullptr };
    };
}


#endif // GREETERAPP_H
