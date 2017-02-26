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

#ifndef XORGSERVER_H
#define XORGSERVER_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

class Launcher;

class XorgServer : public QObject
{
    Q_OBJECT
public:
    explicit XorgServer(Launcher *parent);

    bool isRunning() const;

    QString display() const;
    QString authPath() const;

    bool start();
    void stop();

Q_SIGNALS:
    void started();
    void stopped();

private:
    QString m_display;
    QString m_authPath;
    QString m_cookie;
    QProcess *m_process;
    bool m_started;

    void addCookie(const QString &file);
    void changeOwner(const QString &fileName);

private Q_SLOTS:
    void setupDisplay();
    void finished();
};

#endif // XORGSERVER_H
