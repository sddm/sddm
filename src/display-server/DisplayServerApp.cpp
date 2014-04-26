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

#include "DisplayServerApp.h"

#include "Constants.h"
#include "MessageHandler.h"

#include <QDebug>

#include <iostream>

#include <unistd.h>
#include <wayland-client.h>

namespace SDDM {
    QString parameter(const QStringList &arguments, const QString &key, const QString &defaultValue) {
        int index = arguments.indexOf(key);

        if ((index < 0) || (index >= arguments.size() - 1))
            return defaultValue;

        QString value = arguments.at(index + 1);

        if (value.startsWith("-") || value.startsWith("--"))
            return defaultValue;

        return value;
    }

    DisplayServerApp::DisplayServerApp(int argc, char **argv) : QCoreApplication(argc, argv) {
        // get display name
        m_display = parameter(arguments(), "--display", "");
        if (m_display.isEmpty()) {
            qCritical() << "Missing --display argument";
            exit(EXIT_FAILURE);
        }

        // get socket name
        m_socket = parameter(arguments(), "--socket", "");
        if (m_socket.isEmpty()) {
            qCritical() << "Missing --socket argument";
            exit(EXIT_FAILURE);
        }

        // get theme path
        m_theme = parameter(arguments(), "--theme", "");
        if (m_theme.isEmpty()) {
            qCritical() << "Missing --theme argument";
            exit(EXIT_FAILURE);
        }

        // setup environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("XDG_CONFIG_HOME", env.value("XDG_RUNTIME_DIR"));

        // process
        m_displayServer = new QProcess(this);
        m_displayServer->setProcessEnvironment(env);
        connect(m_displayServer, SIGNAL(started()), this, SLOT(displayServerStarted()));
        connect(m_displayServer, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(displayServerFinished(int)));
        connect(m_displayServer, SIGNAL(readyReadStandardOutput()), this, SLOT(readyReadStandardOutput()));
        connect(m_displayServer, SIGNAL(readyReadStandardError()), this, SLOT(readyReadStandardError()));

        // configure Weston
        configureWeston(env.value("XDG_RUNTIME_DIR"));

        // run Weston
        m_displayServer->setProgram("/usr/bin/weston");
        m_displayServer->setArguments({"--shell=fullscreen-shell.so", QString("--socket=%1").arg(m_display)});
        m_displayServer->start();
        if (!m_displayServer->waitForStarted(5000)) {
            qCritical() << "Failed to start Weston fullscreen shell";
            exit(EXIT_FAILURE);
        }
    }

    void DisplayServerApp::configureWeston(const QString &runtimeDir) {
        // create weston configuration file
        QString fileName = QString("%1/weston.ini").arg(runtimeDir);
        QFile file(fileName);
        if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            file.write("[input-method]\n");
            file.write("path=/bin/true\n");
            file.close();
        } else {
            qCritical() << "Failed to create weston.ini:" << file.errorString();
            exit(EXIT_FAILURE);
        }
    }

    bool DisplayServerApp::waitForStarted(int msecs) {
        bool result = false;

        struct ::wl_display *display = nullptr;

        // try to connect to the compositor
        int i = msecs / 100;
        do {

            // try to connect to the compositor
            display = wl_display_connect(qPrintable(m_display));

            // check display
            if (display != nullptr)
                break;

            // sleep for 100 miliseconds
            usleep(100000);
        } while (i--);

        if (display != nullptr) {
            // disconnect
            wl_display_disconnect(display);

            // set success flag
            result = true;
        }

        // return result
        return result;
    }

    void DisplayServerApp::stopDisplayServer() {
        m_displayServer->terminate();
        if (!m_displayServer->waitForFinished(5000))
            m_displayServer->kill();
        m_displayServer->deleteLater();
    }

    void DisplayServerApp::displayServerStarted() {
        qDebug() << "Weston compositor started";

        // wait for connection
        if (!waitForStarted(5000)) {
            qCritical() << "Can't connect to Weston compositor";
            stopDisplayServer();
            exit(EXIT_FAILURE);
        }

        // setup environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("XDG_VTNR");
        env.insert("WAYLAND_DISPLAY", m_display);

        // prepare process
        m_greeter = new QProcess(this);
        m_greeter->setProcessEnvironment(env);
        connect(m_greeter, SIGNAL(started()), this, SLOT(greeterStarted()));
        connect(m_greeter, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(greeterFinished(int)));

        // run greeter
        m_greeter->setProgram(QString("%1/sddm-greeter").arg(BIN_INSTALL_DIR));
        m_greeter->setArguments({"--socket", m_socket, "--theme", m_theme});
        m_greeter->start();
        if (!m_greeter->waitForStarted(5000)) {
            qCritical() << "Failed to start greeter";
            stopDisplayServer();
            exit(EXIT_FAILURE);
        }
    }

    void DisplayServerApp::displayServerFinished(int status) {
        qDebug() << "Weston compositor stopped";
    }

    void DisplayServerApp::greeterStarted() {
        qDebug() << "Greeter started";
    }

    void DisplayServerApp::greeterFinished(int status) {
        qDebug() << "Greeter stopped";

        m_greeter->terminate();
        if (!m_greeter->waitForFinished(5000))
            m_greeter->terminate();
        m_greeter->deleteLater();

        stopDisplayServer();

        exit(EXIT_SUCCESS);
    }

    void DisplayServerApp::readyReadStandardOutput() {
        std::cout << qPrintable(m_displayServer->readAllStandardOutput());
    }

    void DisplayServerApp::readyReadStandardError() {
        std::cerr << qPrintable(m_displayServer->readAllStandardError());
    }
}

int main(int argc, char **argv) {
    qInstallMessageHandler(SDDM::DaemonMessageHandler);

    QStringList arguments;

    for (int i = 0; i < argc; i++)
        arguments << argv[i];

    if (arguments.contains(QLatin1String("--help")) || arguments.contains(QLatin1String("-h"))) {
        std::cout << "Usage: " << argv[0] << " [options] [arguments]\n"
                     "Options: \n"
                     "  --display <display name>   Set display name\n"
                     "  --theme <theme path>       Set greeter theme\n"
                     "  --socket <socket name>     Set socket name" << std::endl;
        return EXIT_FAILURE;
    }

    // create application
    SDDM::DisplayServerApp app(argc, argv);

    // run application
    return app.exec();
}
