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

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

class Process;
class XorgServer;

namespace SDDM {
class SignalHandler;
}

class Launcher : public QObject
{
    Q_OBJECT
public:
    explicit Launcher(QObject *parent = nullptr);

    bool isRootless() const;

    bool isTestModeEnabled() const;
    void setTestModeEnabled(bool on);

    QString socket() const;
    void setSocket(const QString &socket);

    QString session() const;
    void setSession(const QString &session);

    bool start();
    void stop();

private:
    bool m_rootless;
    bool m_testMode;
    QString m_socket;
    QString m_session;
    XorgServer *m_server;
    Process *m_process;
    SDDM::SignalHandler *m_signalHandler;
};

#endif // LAUNCHER_H
