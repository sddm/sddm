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

#ifndef PROCESS_H
#define PROCESS_H

#include <QtCore/QObject>
#include <QtCore/QMap>

class Launcher;

class Process : public QObject
{
    Q_OBJECT
public:
    explicit Process(Launcher *parent);

    bool isRunning() const;

    void setEnvironment(const QString &key, const QString &value);
    void removeEnvironment(const QString &key);

    virtual bool start() = 0;

public Q_SLOTS:
    virtual void stop() = 0;

Q_SIGNALS:
    void started();
    void stopped();

protected:
    bool m_started;
    QMap<QString, QString> m_env;
};

#endif // PROCESS_H
