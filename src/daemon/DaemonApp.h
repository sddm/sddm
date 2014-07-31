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

#ifndef SDDM_DAEMONAPP_H
#define SDDM_DAEMONAPP_H

#include <QCoreApplication>

#define daemonApp DaemonApp::instance()

namespace SDDM {
    class Configuration;
    class DisplayManager;
    class PowerManager;
    class SeatManager;
    class SignalHandler;

    class DaemonApp : public QCoreApplication {
        Q_OBJECT
        Q_DISABLE_COPY(DaemonApp)
    public:
        explicit DaemonApp(int &argc, char **argv);

        static DaemonApp *instance() { return self; }

        // TODO: move these two away
        bool testing() const;
        bool first { true };

        QString hostName() const;
        DisplayManager *displayManager() const;
        PowerManager *powerManager() const;
        SeatManager *seatManager() const;
        SignalHandler *signalHandler() const;

    public slots:
        int newSessionId();

    private:
        static DaemonApp *self;

        int m_lastSessionId { 0 };

        bool m_testing { false };
        DisplayManager *m_displayManager { nullptr };
        PowerManager *m_powerManager { nullptr };
        SeatManager *m_seatManager { nullptr };
        SignalHandler *m_signalHandler { nullptr };
    };
}

#endif // SDDM_DAEMONAPP_H
