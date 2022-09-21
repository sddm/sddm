/*
 * Session process wrapper
 * Copyright (C) 2021 Aleix Pol Gonzalez <aleixpol@kde.org>
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

/**
 * This application sole purpose is to launch an X11 rootless compositor compositor (first
 * argument) and as soon as it's set up to launch a client (second argument)
 */

#include <unistd.h>
#include <QCoreApplication>
#include <QTextStream>
#include <QProcess>
#include <QDebug>
#include "xorguserhelper.h"
#include "MessageHandler.h"
#include <signal.h>
#include "SignalHandler.h"

void X11UserHelperMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    SDDM::messageHandler(type, context, QStringLiteral("X11UserHelper: "), msg);
}

int main(int argc, char** argv)
{
    qInstallMessageHandler(X11UserHelperMessageHandler);
    QCoreApplication app(argc, argv);
    SDDM::SignalHandler s;
    QObject::connect(&s, &SDDM::SignalHandler::sigtermReceived, &app, [] {
        QCoreApplication::instance()->exit(-1);
    });

    Q_ASSERT(::getuid() != 0);
    if (argc != 3) {
        QTextStream(stderr) << "Wrong number of arguments\n";
        return 33;
    }

    using namespace SDDM;
    XOrgUserHelper helper;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &helper, [&helper] {
        qDebug("quitting helper-start-x11");
        helper.stop();
    });
    QObject::connect(&helper, &XOrgUserHelper::displayChanged, &app, [&helper, &app] {
        qDebug() << "starting XOrg Greeter..." << helper.sessionEnvironment().value(QStringLiteral("DISPLAY"));
        auto args = QProcess::splitCommand(app.arguments()[2]);

        QProcess *process = new QProcess(&app);
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->setProgram(args.takeFirst());
        process->setArguments(args);
        process->setProcessEnvironment(helper.sessionEnvironment());
        process->start();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &app, &QCoreApplication::quit);
    });

    helper.start(app.arguments()[1]);
    return app.exec();
}
