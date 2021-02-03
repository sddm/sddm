/*
 * Session process wrapper
 * Copyright (C) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#ifndef SDDM_WAYLAND_SESSION_WATCHER_H
#define SDDM_WAYLAND_SESSION_WATCHER_H

#include <QObject>
#include <QTimer>
#include <QPointer>

class QFileSystemWatcher;
class QProcessEnvironment;

namespace SDDM {

    class WaylandSocketWatcher : public QObject
    {
        Q_OBJECT
    public:
        WaylandSocketWatcher(QObject* parent, const QProcessEnvironment &env);

        void start();
        void stop();

    Q_SIGNALS:
        void sessionStarted(bool started);

    private:
        QPointer<QFileSystemWatcher> m_watcher;
        QTimer m_timer;
        QString m_runtimeDir;
        QString m_socketFileName;
    };
}

#endif
