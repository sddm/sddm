/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QCoreApplication>
#include <QDebug>

#include "SignalHandler.h"

#include "waylanduserhelper.h"

using namespace SDDM;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("sddm-helper-wayland"));
    app.setApplicationVersion(QStringLiteral(SDDM_VERSION));
    app.setOrganizationName(QStringLiteral("SDDM"));

    // QCommandLineParser fails to parse arguments like these, so
    // we manually do it ourselves
    const auto args = QCoreApplication::arguments();
    int pos;
    QString fdArg, serverCmd, clientCmd;

    if ((pos = args.indexOf(QStringLiteral("--fd"))) >= 0) {
        if (pos >= args.length() - 1) {
            qCritical("This application is not supposed to be executed manually");
            return EXIT_FAILURE;
        }
        fdArg = args[pos + 1];
    }

    if ((pos = args.indexOf(QStringLiteral("--server"))) >= 0) {
        if (pos >= args.length() - 1) {
            qCritical("This application is not supposed to be executed manually");
            return EXIT_FAILURE;
        }
        serverCmd = args[pos + 1];
    }

    if ((pos = args.indexOf(QStringLiteral("--client"))) >= 0) {
        if (pos >= args.length() - 1) {
            qCritical("This application is not supposed to be executed manually");
            return EXIT_FAILURE;
        }
        clientCmd = args[pos + 1];
    }

    if (serverCmd.isEmpty() || clientCmd.isEmpty()) {
        qCritical("This application is not supposed to be executed manually");
        return EXIT_FAILURE;
    }

    int fd = -1;
    if (!fdArg.isEmpty()) {
        auto fdOk = false;
        fd = fdArg.toInt(&fdOk);
        if (!fdOk) {;
            qCritical("This application is not supposed to be executed manually");
            return EXIT_FAILURE;
        }
    }

    auto *signalHandler = new SignalHandler(&app);
    signalHandler->initialize();
    QObject::connect(signalHandler, &SignalHandler::sigintReceived, &QCoreApplication::quit);
    QObject::connect(signalHandler, &SignalHandler::sigtermReceived, &QCoreApplication::quit);

    auto *helper = new WaylandUserHelper(fd, serverCmd, clientCmd);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [helper] {
        qInfo("Quitting...");
        helper->stop();
        helper->deleteLater();
    });

    QCoreApplication::postEvent(helper, new StartupEvent());

    return app.exec();
}
