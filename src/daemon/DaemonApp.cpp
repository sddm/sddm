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

#include "Configuration.h"
#include "Constants.h"
#include "DisplayManager.h"
#include "PowerManager.h"
#include "SeatManager.h"
#include "SignalHandler.h"

#ifdef USE_QT5
#include "MessageHandler.h"
#endif

#include <QDebug>
#include <QHostInfo>
#include <QTimer>
#include <QFile>

#include <iostream>

namespace SDDM {
    DaemonApp *DaemonApp::self = nullptr;

    DaemonApp::DaemonApp(int argc, char **argv) : QCoreApplication(argc, argv) {
        // point instance to this
        self = this;

#ifdef USE_QT5
        qInstallMessageHandler(SDDM::MessageHandler);
#endif

        // log message
        qDebug() << " DAEMON: Initializing...";

        // Write PID File
        if (! QString(PID_FILE).isEmpty()) {
            QFile pidFile(PID_FILE);
            QString pid = QString::number(QCoreApplication::applicationPid());
            if ( pidFile.open(QIODevice::WriteOnly | QIODevice::Text) ) {
                pidFile.write(pid.toLatin1().data(), qstrlen(pid.toLatin1().data()));
                pidFile.close();
            }
        }

        // create configuration
        m_configuration = new Configuration(CONFIG_FILE, this);

        // set testing parameter
        m_configuration->testing = (arguments().indexOf("--test-mode") != -1);

        // create display manager
        m_displayManager = new DisplayManager(this);

        // create power manager
        m_powerManager = new PowerManager(this);

        // create seat manager
        m_seatManager = new SeatManager(this);

        // connect with display manager
        connect(m_seatManager, SIGNAL(seatCreated(QString)), m_displayManager, SLOT(AddSeat(QString)));
        connect(m_seatManager, SIGNAL(seatRemoved(QString)), m_displayManager, SLOT(RemoveSeat(QString)));

        // create signal handler
        SignalHandler *signalHandler = new SignalHandler(this);

        // initialize signal signalHandler
        SignalHandler::initialize();

        // quit when SIGHUP, SIGINT, SIGTERM received
        connect(signalHandler, SIGNAL(sighupReceived()), this, SLOT(self.quit()));
        connect(signalHandler, SIGNAL(sigintReceived()), this, SLOT(self.quit()));
        connect(signalHandler, SIGNAL(sigtermReceived()), this, SLOT(self.quit()));

        // log message
        qDebug() << " DAEMON: Starting...";

        // add a seat
        m_seatManager->createSeat("seat0");
    }

    void DaemonApp::quit() {
	qDebug() << " DAEMON: Stopping...";
        // Delete PID File If it Exists
        if ( ! QString(PID_FILE).isEmpty() )  {
            QFile::remove(PID_FILE);
        }
        QCoreApplication::quit();
    }

    QString DaemonApp::hostName() const {
        return QHostInfo::localHostName();
    }

    Configuration *DaemonApp::configuration() const {
        return m_configuration;
    }

    DisplayManager *DaemonApp::displayManager() const {
        return m_displayManager;
    }

    PowerManager *DaemonApp::powerManager() const {
        return m_powerManager;
    }

    SeatManager *DaemonApp::seatManager() const {
        return m_seatManager;
    }

    int DaemonApp::newSessionId() {
        return m_lastSessionId++;
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

        return EXIT_FAILURE;
    }

    // create application
    SDDM::DaemonApp app(argc, argv);

    // run application
    return app.exec();
}
