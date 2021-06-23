/*
 * Session process wrapper
 * Copyright (C) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#ifndef SDDM_AUTH_SESSION_H
#define SDDM_AUTH_SESSION_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

namespace SDDM {
    class HelperApp;
    class XOrgUserHelper;
    class WaylandHelper;
    class UserSession : public QObject
    {
        Q_OBJECT
    public:
        explicit UserSession(HelperApp *parent);

        bool start();
        void stop();
        void terminate();

        QProcessEnvironment processEnvironment() const;
        void setProcessEnvironment(const QProcessEnvironment &env);

        QString displayServerCommand() const;
        void setDisplayServerCommand(const QString &command);

        void setPath(const QString &path);
        QString path() const;

        qint64 processId() const;

    Q_SIGNALS:
        void finished(int exitCode);

    private:
        void setup();

        QString m_path { };
        QProcess *m_process = nullptr;
        XOrgUserHelper *m_xorgUser = nullptr;
        WaylandHelper *m_wayland = nullptr;
        QString m_displayServerCmd;
    };
}

#endif // SDDM_AUTH_SESSION_H
