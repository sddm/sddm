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

#ifndef WAYLANDUSERHELPER_H
#define WAYLANDUSERHELPER_H

#include <QEvent>
#include <QProcess>

namespace SDDM {

static const QEvent::Type StartupEventType =
        static_cast<QEvent::Type>(QEvent::registerEventType());

class StartupEvent : public QEvent
{
public:
    StartupEvent();
};

class WaylandUserHelper : public QObject
{
    Q_OBJECT
public:
    explicit WaylandUserHelper(int fd, const QString &serverCmd, const QString &clientCmd,
                               QObject *parent = nullptr);

    bool start();
    void stop();

protected:
    void customEvent(QEvent *event) override;

private:
    int m_fd = -1;
    QString m_serverCmd;
    QString m_clientCmd;
    QString m_display = QStringLiteral("wayland-0");
    QProcess *m_serverProcess = nullptr;
    QProcess *m_clientProcess = nullptr;

    bool startProcess(const QString &cmd, const QProcessEnvironment &env,
                      QProcess **p = nullptr);
    bool startServer();
    bool startClient();
};

} // namespace SDDM

#endif // WAYLANDUSERHELPER_H
