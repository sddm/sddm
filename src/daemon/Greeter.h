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

#ifndef SDDM_GREETER_H
#define SDDM_GREETER_H

#include <QObject>

#include "Auth.h"

class QProcess;

namespace SDDM {
    class Display;
    class ThemeMetadata;
    class ThemeConfig;

    class Greeter : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Greeter)
    public:
        explicit Greeter(Display *parent = 0);
        ~Greeter();

        void setSocket(const QString &socket);
        void setTheme(const QString &theme);

        QString displayServerCommand() const;
        void setDisplayServerCommand(const QString &cmd);
        bool isRunning() const;

    public slots:
        bool start();
        void stop();
        void finished();

    private slots:
        void onRequestChanged();
        void onSessionStarted(bool success);
        void onDisplayServerReady(const QString &displayName);
        void onHelperFinished(Auth::HelperExitStatus status);
        void onReadyReadStandardOutput();
        void onReadyReadStandardError();
        void authInfo(const QString &message, Auth::Info info);
        void authError(const QString &message, Auth::Error error);

    signals:
        void ttyFailed();
        void failed();
        void displayServerFailed();

    private:
        bool m_started { false };

        Display * const m_display { nullptr };
        QString m_socket;
        QString m_themePath;
        QString m_displayServerCmd;
        ThemeMetadata *m_metadata { nullptr };
        ThemeConfig *m_themeConfig { nullptr };

        Auth *m_auth { nullptr };
        QProcess *m_process { nullptr };

        static void insertEnvironmentList(QStringList names, QProcessEnvironment sourceEnv, QProcessEnvironment &targetEnv);
    };
}

#endif // SDDM_GREETER_H
