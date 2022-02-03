/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef WAYLANDSOCKETWATCHER_H
#define WAYLANDSOCKETWATCHER_H

#include <QDir>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QTimer>

namespace SDDM {

class WaylandSocketWatcher : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Started,
        Stopped,
        Failed
    };
    Q_ENUM(Status)

    explicit WaylandSocketWatcher(QObject *parent = nullptr);

    Status status() const;
    QString socketName() const;

    void start();
    void stop();

Q_SIGNALS:
    void started();
    void stopped();
    void failed();

private:
    Status m_status = Stopped;
    QDir m_runtimeDir;
    QString m_socketName;
    QTimer m_timer;
    QPointer<QFileSystemWatcher> m_watcher;
};

} // namespace SDDM

#endif // WAYLANDSOCKETWATCHER_H
