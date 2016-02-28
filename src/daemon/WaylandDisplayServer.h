/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_WAYLANDDISPLAYSERVER_H
#define SDDM_WAYLANDDISPLAYSERVER_H

#include "Auth.h"
#include "DisplayServer.h"

class QProcess;

namespace SDDM
{
    class WaylandDisplayServer : public DisplayServer
    {
        Q_OBJECT
        Q_DISABLE_COPY(WaylandDisplayServer)
    public:
        explicit WaylandDisplayServer(Display *parent);
        ~WaylandDisplayServer();

        const QString &display() const;

        QString sessionType() const;

    public slots:
        bool start();
        void stop();
        void finished();
        void setupDisplay();

    private slots:
        void onRequestChanged();
        void onSessionStarted(bool success);
        void onHelperFinished(Auth::HelperExitStatus status);
        void onReadyReadStandardOutput();
        void onReadyReadStandardError();
        void authInfo(const QString &message, Auth::Info info);
        void authError(const QString &message, Auth::Error error);

    private:
        QProcess *m_process { nullptr };
        Auth *m_auth { nullptr };
    };
}

#endif // SDDM_WAYLANDDISPLAYSERVER_H
