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
#include "Display.h"
#include "PowerManager.h"
#include "SignalHandler.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include "MessageHandler.h"
#endif

#include <QDebug>
#include <QHostInfo>
#include <QProcess>
#include <QTimer>

namespace SDDM {
    DaemonApp::DaemonApp(int argc, char **argv) : QCoreApplication(argc, argv) {
        // log message
        qDebug() << " DAEMON: Initializing...";

        // create configuration
        m_configuration = new Configuration(CONFIG_FILE, this);

        // set testing parameter
        m_configuration->testing = (arguments().indexOf("--test-mode") != -1);

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

        // start the main loop
        QTimer::singleShot(1, this, SLOT(start()));

        // add a display
        addDisplay();
    }

    QString DaemonApp::hostName() const {
        return QHostInfo::localHostName();
    }

    PowerManager *DaemonApp::powerManager() const {
        return m_powerManager;
    }

    void DaemonApp::start() {
        // log message
        qDebug() << " DAEMON: Starting...";

        // start all displays
        for (Display *display: m_displays)
            display->start();
    }

    void DaemonApp::stop() {
        qDebug() << " DAEMON: Stopping...";

        // stop all displays
        for (Display *display: m_displays)
            display->stop();

        // quit application
        qApp->quit();
    }

    void DaemonApp::addDisplay() {
        // find unused display
        int d = findUnused(m_usedDisplays, 0);

        // find unused vt
        int vtNumber = findUnused(m_usedVTs, Configuration::instance()->minimumVT);

        // log message
        qDebug() << " DAEMON: Adding new display " << QString(":%1").arg(d) << " on vt" << vtNumber << "...";

        // create a new display
        Display *display = new Display(d, vtNumber, this);

        // add display to the list
        m_displays << display;
    }

    void DaemonApp::removeDisplay() {
        Display *display = qobject_cast<Display *>(sender());

        // log message
        qDebug() << " DAEMON: Removing display" << display->name() << "...";

        // remove display from list
        m_displays.removeAll(display);

        // mark display and vt numbers as unused
        m_usedDisplays.removeAll(display->displayNumber());
        m_usedVTs.removeAll(display->vtNumber());

        // delete display
        display->deleteLater();
    }

    int DaemonApp::findUnused(QList<int> &used, int minimum) {
        // initialize with minimum
        int number = minimum;

        // find unused
        while (used.contains(number))
            number++;

        // mark number as used
        used << number;

        // return number;
        return number;
    }
}

int main(int argc, char **argv) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // install message handler
    qInstallMessageHandler(SDDM::MessageHandler);
#endif

    // create application
    SDDM::DaemonApp app(argc, argv);

    // run application
    return app.exec();
}
