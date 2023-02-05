/***************************************************************************
* Copyright (c) 2021 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#ifndef WAYLANDHELPER_H
#define WAYLANDHELPER_H

#include <QProcess>

namespace SDDM {
class WaylandSocketWatcher;

class WaylandHelper : public QObject
{
    Q_OBJECT
public:
    explicit WaylandHelper(QObject *parent = nullptr);

    bool startCompositor(const QString &cmd);
    void startGreeter(const QString &cmd);
    void stop();

Q_SIGNALS:
    void failed();

private:
    QProcessEnvironment m_environment;
    QProcess *m_serverProcess = nullptr;
    QProcess *m_greeterProcess = nullptr;
    WaylandSocketWatcher * const m_watcher;

    bool startProcess(const QString &cmd, QProcess **p = nullptr);
};

} // namespace SDDM



#endif
