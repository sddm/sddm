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

#include <QDBusObjectPath>
#include <QList>

#define daemonApp DaemonApp::instance()

namespace SDDM {
    class Configuration;
    class PowerManager;
    class Seat;

    class DaemonApp : public QCoreApplication {
        Q_OBJECT
        Q_DISABLE_COPY(DaemonApp)
        Q_PROPERTY(QList<QDBusObjectPath> Seats READ Seats CONSTANT)
        Q_PROPERTY(QList<QDBusObjectPath> Sessions READ Sessions CONSTANT)
    public:
        explicit DaemonApp(int argc, char **argv);

        static DaemonApp *instance() { return self; };

        QString hostName() const;
        PowerManager *powerManager() const;

    public slots:
        void start();
        void stop();

        int newSessionId();

        QList<QDBusObjectPath> Seats() const;
        QList<QDBusObjectPath> Sessions() const;

    private:
        static DaemonApp *self;

        QList<Seat *> m_seats;
        int m_lastSessionId { 0 };
        Configuration *m_configuration { nullptr };
        PowerManager *m_powerManager { nullptr };
    };
}

#endif // SDDM_DAEMONAPP_H
