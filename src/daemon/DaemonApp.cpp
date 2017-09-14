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

#include "MessageHandler.h"

#include <QDebug>
#include <QHostInfo>
#include <QTimer>

#include <iostream>

namespace SDDM {
    DaemonApp *DaemonApp::self = nullptr;

    DaemonApp::DaemonApp(int &argc, char **argv) : QCoreApplication(argc, argv) {
        // point instance to this
        self = this;

        qInstallMessageHandler(SDDM::DaemonMessageHandler);

        // log message
        qDebug() << "Initializing...";

        // set testing parameter
        m_testing = (arguments().indexOf(QStringLiteral("--test-mode")) != -1);

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
        m_signalHandler = new SignalHandler(this);

        // initialize signal signalHandler
        SignalHandler::initialize();

        // quit when SIGHUP, SIGINT, SIGTERM received
        connect(m_signalHandler, SIGNAL(sighupReceived()), this, SLOT(quit()));
        connect(m_signalHandler, SIGNAL(sigintReceived()), this, SLOT(quit()));
        connect(m_signalHandler, SIGNAL(sigtermReceived()), this, SLOT(quit()));

        // log message
        qDebug() << "Starting...";
    }

    bool DaemonApp::testing() const {
        return m_testing;
    }


    QString DaemonApp::hostName() const {
        return QHostInfo::localHostName();
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

    SignalHandler *DaemonApp::signalHandler() const {
        return m_signalHandler;
    }

    int DaemonApp::newSessionId() {
        return m_lastSessionId++;
    }
}

int main(int argc, char **argv) {
    QStringList arguments;

    for (int i = 0; i < argc; i++)
        arguments << QString::fromLocal8Bit(argv[i]);

    if (arguments.contains(QStringLiteral("--help")) || arguments.contains(QStringLiteral("-h"))) {
        std::cout << "Usage: sddm [options]\n"
                  << "Options: \n"
                  << "  --test-mode         Start daemon in test mode" << std::endl
                  << "  --example-config    Print the complete current configuration to stdout" << std::endl;

        return EXIT_FAILURE;
    }

    // spit a complete config file on stdout and quit on demand
    if (arguments.contains(QStringLiteral("--example-config"))) {
        SDDM::mainConfig.wipe();
        QTextStream(stdout) << SDDM::mainConfig.toConfigFull();
        return EXIT_SUCCESS;
    }

    // create application
    SDDM::DaemonApp app(argc, argv);

    // run application
    return app.exec();
}
