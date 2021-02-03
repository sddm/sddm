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
 * This application sole purpose is to launch a wayland compositor (first
 * argument) and as soon as it's set up to launch a client (second argument)
 */

#include <QCoreApplication>
#include <QTextStream>
#include <QProcess>
#include "WaylandSocketWatcher.h"

bool startProcess(const QString &cmd, QProcess** process = nullptr)
{
    QStringList args = QProcess::splitCommand(cmd);
    const QString program = args.takeFirst();

    auto app = QCoreApplication::instance();
    QProcess* p = new QProcess(app);
    p->setProcessChannelMode(QProcess::ForwardedChannels);
    QObject::connect(app, &QCoreApplication::aboutToQuit, p, [p] {
        p->terminate();
        if (!p->waitForFinished(5000)) {
            p->kill();
        }
    });
    p->start(program, args);
    if (!p->waitForStarted(10000)) {
        QTextStream(stderr) << "Failed to start: " << cmd << ". " << p->errorString() << Qt::endl;
        return false;
    }
    if (process) {
        *process = p;
    }
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] {
        qDebug("quitting helper-start-wayland");
    });
    if (argc != 3) {
        QTextStream(stderr) << "Wrong number of arguments\n";
        return 33;
    }
    const QString compositor = app.arguments()[1];
    const QString client = app.arguments()[2];

    auto watcher = new SDDM::WaylandSocketWatcher(&app, QProcessEnvironment::systemEnvironment());
    QObject::connect(watcher, &SDDM::WaylandSocketWatcher::sessionStarted, &app, [client, &app] (bool started) {
        if (!started) {
            QTextStream(stderr) << "Failed to start session: " << client << Qt::endl;
            app.exit(35);
            return;
        }

        QTextStream(stdout) << "Starting client: " << client << Qt::endl;
        QProcess* clientProcess;
        if (!startProcess(client, &clientProcess)) {
            app.exit(36);
            return;
        }
        QObject::connect(clientProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &app, [] (int exitCode, QProcess::ExitStatus exitStatus) {
            QTextStream(exitStatus == QProcess::NormalExit ? stdout : stderr) << "Greeter finished with code: " << exitCode << Qt::endl;
            QCoreApplication::instance()->exit(exitCode);
        });
    });
    watcher->start();

    QTextStream(stdout) << "Starting compositor: " << compositor << Qt::endl;
    if (!startProcess(compositor)) {
        return 34;
    }

    return app.exec();
}
