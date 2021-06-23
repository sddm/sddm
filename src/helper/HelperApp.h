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

#ifndef HELPERAPP_H
#define HELPERAPP_H

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
        const QString &cookie() const;

    public slots:
        Request request(const Request &request, bool &cancel);
        void info(const QString &message, AuthEnums::Info type, int result);
        void error(const QString &message, AuthEnums::Error type, int result);
        QProcessEnvironment authenticated(const QString &user);
        void sessionOpened(bool success, qint64 pid);
        void displayServerStarted(const QString &displayName);

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
        QString m_cookie { };

    };
}

#endif // HELPERAPP_H
