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

using namespace std;

namespace SDDM {
    DaemonApp::DaemonApp(int argc, char **argv) : QCoreApplication(argc, argv) {
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
        m_signalHandler = new SignalHandler(this);

        // initialize signal signalHandler
        SignalHandler::initialize();

        // quit when SIGHUP, SIGINT, SIGTERM received
        connect(m_signalHandler, SIGNAL(sighupReceived()), this, SLOT(stop()));
        connect(m_signalHandler, SIGNAL(sigintReceived()), this, SLOT(stop()));
        connect(m_signalHandler, SIGNAL(sigtermReceived()), this, SLOT(stop()));

        // schedule start
        QTimer::singleShot(1, this, SLOT(start()));
    }

    QString DaemonApp::hostName() const {
        return QHostInfo::localHostName();
    }

    PowerManager *DaemonApp::powerManager() const {
        return m_powerManager;
    }

    void DaemonApp::start() {
        qDebug() << " DAEMON: Starting...";

        // add default seat
        Seat *seat = new Seat("0", this);

        // add seat to the list
        m_seats << seat;

        // start seat
        seat->start();
    }

    void DaemonApp::stop() {
        qDebug() << " DAEMON: Stopping...";

        // stop all seats
        for (Seat *seat : m_seats)
            seat->stop();

        qDebug() << " DAEMON: Stopped.";

        // quit application
        qApp->quit();
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

void showUsageHelp(const char*  appName) {
    cout << "Usage: " << appName << " [options] [arguments]\n"
         << "Options: \n"
         << "  --test-mode         Start daemon in test mode" << endl;
}

int main(int argc, char **argv) {
    QStringList arguments;

    for(int ii = 0; ii < argc; ii++) {
        arguments << argv[ii];
    }

    if ( arguments.indexOf("--help") > 0 || arguments.indexOf("-h") > 0 ) {
        showUsageHelp(argv[0]);
        return 1;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // install message handler
    qInstallMessageHandler(SDDM::MessageHandler);
#endif

    // create application
    SDDM::DaemonApp app(argc, argv);

    // run application
    return app.exec();
}
