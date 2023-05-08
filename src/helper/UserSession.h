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
#include <QtCore/QTemporaryFile>

namespace SDDM {
    class HelperApp;
    class XOrgUserHelper;
    class WaylandHelper;
    class UserSession : public QProcess
    {
        Q_OBJECT
    public:
        explicit UserSession(HelperApp *parent);

        bool start();
        void stop();

        QString displayServerCommand() const;
        void setDisplayServerCommand(const QString &command);

        void setPath(const QString &path);
        QString path() const;

        /*!
         \brief Gets m_cachedProcessId
         \return  The cached process ID
        */
        qint64 cachedProcessId();


    Q_SIGNALS:
        void finished(int exitCode);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    protected:
        void setupChildProcess() override;
#endif

    private:
        void setup();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Don't call it directly, it will be invoked by the child process only
        void childModifier();
#endif

        QString m_path { };
        QTemporaryFile m_xauthFile;
        QString m_displayServerCmd;

        /*!
         Needed for getting the PID of a finished UserSession and calling HelperApp::utmpLogout
        */
        qint64 m_cachedProcessId = -1;
    };
}

#endif // SDDM_AUTH_SESSION_H
