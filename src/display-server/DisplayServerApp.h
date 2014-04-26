/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_DISPLAYSERVER_APP_H
#define SDDM_DISPLAYSERVER_APP_H

#include <QCoreApplication>
#include <QProcess>

namespace SDDM {
    class DisplayServerApp : public QCoreApplication {
        Q_OBJECT
        Q_DISABLE_COPY(DisplayServerApp)
    public:
        explicit DisplayServerApp(int argc, char **argv);

    private:
        QString m_display;
        QString m_socket;
        QString m_theme;

        QProcess *m_displayServer { nullptr };
        QProcess *m_greeter { nullptr };

        void configureWeston(const QString &runtimeDir);
        bool waitForStarted(int msecs = 5000);
        void stopDisplayServer();

    private slots:
        void displayServerStarted();
        void displayServerFinished(int status);

        void greeterStarted();
        void greeterFinished(int status);

        void readyReadStandardOutput();
        void readyReadStandardError();
    };
}

#endif // SDDM_DISPLAYSERVER_APP_H
