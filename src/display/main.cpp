/*
 * This file is part of SDDM.
 *
 * Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>

#include "launcher.h"

#define TR(x) QCoreApplication::translate("main", x)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QLatin1String("sddm-display"));

    QCommandLineParser parser;
    parser.setApplicationDescription(TR("Run display server and SDDM greeter"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption testModeOption(QLatin1String("test-mode"),
                                      TR("Start display in test mode."));
    parser.addOption(testModeOption);

    QCommandLineOption sessionOption(QLatin1String("session"),
                                     TR("Set command to run instead of greeter to <session>."),
                                     TR("session"));
    parser.addOption(sessionOption);

    QCommandLineOption socketOption(QLatin1String("socket"),
                                    TR("Set daemon socket to <socket>."),
                                    TR("socket"));
    parser.addOption(socketOption);

    parser.process(app);

    qInfo("sddm-display started");

    Launcher *session = new Launcher();
    session->setTestModeEnabled(parser.isSet(testModeOption));
    session->setSocket(parser.value(socketOption));
    session->setSession(parser.value(sessionOption));
    session->start();

    return app.exec();
}
