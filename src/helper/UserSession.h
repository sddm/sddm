/*
 * Session process wrapper
 * Copyright (C) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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
#include <QtCore/QString>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

namespace SDDM {
    class HelperApp;
    class UserSession : public QProcess
    {
        Q_OBJECT
    public:
        explicit UserSession(HelperApp *parent);
        virtual ~UserSession();

        bool start();

        bool isDisplayServer() const;
        void setDisplayServer(bool value);

        void setPath(const QString &path);
        QString path() const;

        /*!
         \brief Sets m_cachedProcessId. Needed for getting the PID of a finished UserSession
                and calling HelperApp::utmpLogout
         \param pid  The process ID
        */
        void setCachedProcessId(qint64 pid);

        /*!
         \brief Gets m_cachedProcessId
         \return  The cached process ID
        */
        qint64 cachedProcessId();

    signals:
        void sessionStarted(bool success);

    protected:
        void setupChildProcess();

    private:
        bool m_displayServer { false };
        QString m_path { };
        qint64 m_cachedProcessId;
        QTimer m_timer;
        QPointer<QFileSystemWatcher> m_watcher;
    };
}

#endif // SDDM_AUTH_SESSION_H
