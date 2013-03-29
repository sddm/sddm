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

#include "DaemonApp.h"

#include "displaymanageradaptor.h"

#include "Configuration.h"
#include "Constants.h"
#include "PowerManager.h"
#include "Seat.h"
#include "SignalHandler.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include "MessageHandler.h"
#endif

#include <QDebug>
#include <QHostInfo>
#include <QTimer>

#include <iostream>

namespace SDDM {
    DaemonApp *DaemonApp::self = nullptr;

    DaemonApp::DaemonApp(int argc, char **argv) : QCoreApplication(argc, argv) {
        // point instance to this
        self = this;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    qInstallMessageHandler(SDDM::MessageHandler);
#endif

        // log message
        qDebug() << " DAEMON: Initializing...";

        // create configuration
        m_configuration = new Configuration(CONFIG_FILE, this);

        // set testing parameter
        m_configuration->testing = (arguments().indexOf("--test-mode") != -1);

        // create display manager adapter
        new DisplayManagerAdaptor(this);
        QDBusConnection connection = (Configuration::instance()->testing) ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
        connection.registerService("org.freedesktop.DisplayManager");
        connection.registerObject("/org/freedesktop/DisplayManager", this);

        // create power manager
        m_powerManager = new PowerManager(this);

        // create signal handler
        SignalHandler *signalHandler = new SignalHandler(this);

        // initialize signal signalHandler
        SignalHandler::initialize();

        // quit when SIGHUP, SIGINT, SIGTERM received
        connect(signalHandler, SIGNAL(sighupReceived()), this, SLOT(quit()));
        connect(signalHandler, SIGNAL(sigintReceived()), this, SLOT(quit()));
        connect(signalHandler, SIGNAL(sigtermReceived()), this, SLOT(quit()));

        // log message
        qDebug() << " DAEMON: Starting...";

        // add a seat
        m_seats << new Seat("0", this);

        // start seat
        m_seats.first()->start();
    }

    QString DaemonApp::hostName() const {
        return QHostInfo::localHostName();
    }

    PowerManager *DaemonApp::powerManager() const {
        return m_powerManager;
    }

    int DaemonApp::newSessionId() {
        return m_lastSessionId++;
    }

    QList<QDBusObjectPath> DaemonApp::Seats() const {
        QList<QDBusObjectPath> seatPaths;

        for (Seat *seat: m_seats)
            seatPaths << QDBusObjectPath(seat->path());

        return seatPaths;
    }

    QList<QDBusObjectPath> DaemonApp::Sessions() const {
        QList<QDBusObjectPath> sessionPaths;

        for (Seat *seat: m_seats)
            sessionPaths << seat->Sessions();

        return sessionPaths;
    }
}

int main(int argc, char **argv) {
    QStringList arguments;

    for (int i = 0; i < argc; i++)
        arguments << argv[i];

    if (arguments.contains(QLatin1String("--help")) || arguments.contains(QLatin1String("-h"))) {
        std::cout << "Usage: sddm [options]\n"
                  << "Options: \n"
                  << "  --test-mode         Start daemon in test mode" << std::endl;

        return 1;
    }

    // create application
    SDDM::DaemonApp app(argc, argv);

    // run application
    return app.exec();
}
