/*
 * Main authentication application class
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
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

#ifndef Auth_H
#define Auth_H

#include <QtCore/QCoreApplication>
#include <QtCore/QProcessEnvironment>

#include "AuthMessages.h"

class QLocalSocket;

namespace SDDM {
    class Backend;
    class UserSession;
    class HelperApp : public QCoreApplication
    {
        Q_OBJECT
    public:
        HelperApp(int& argc, char** argv);
        virtual ~HelperApp();

        UserSession *session();
        const QString &user() const;
        const QByteArray &cookie() const;

    public slots:
        Request request(const Request &request);
        void info(const QString &message, Auth::Info type);
        void error(const QString &message, Auth::Error type);
        QProcessEnvironment authenticated(const QString &user);
        void displayServerStarted(const QString &displayName);
        void sessionOpened(bool success);

    private slots:
        void setUp();
        void doAuth();

        void sessionFinished(int status);

    private:
        qint64 m_id { -1 };
        Backend *m_backend { nullptr };
        UserSession *m_session { nullptr };
        QLocalSocket *m_socket { nullptr };
        QString m_user { };
        // TODO: get rid of this in a nice clean way along the way with moving to user session X server
        QByteArray m_cookie { };

        /*!
         \brief Write utmp/wtmp/btmp records when a user logs in
         \param vt  Virtual terminal (tty7, tty8,...)
         \param displayName  Display (:0, :1,...)
         \param user  User logging in
         \param pid  User process ID (e.g. PID of startkde)
         \param authSuccessful  Was authentication successful
        */
        void utmpLogin(const QString &vt, const QString &displayName, const QString &user, qint64 pid, bool authSuccessful);

        /*!
         \brief Write utmp/wtmp records when a user logs out
         \param vt  Virtual terminal (tty7, tty8,...)
         \param displayName  Display (:0, :1,...)
         \param pid  User process ID (e.g. PID of startkde)
        */
        void utmpLogout(const QString &vt, const QString &displayName, qint64 pid);
    };
}

#endif // Auth_H
