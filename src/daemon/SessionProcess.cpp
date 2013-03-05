/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#include "SessionProcess.h"

#include "Authenticator.h"
#include "Constants.h"

#include <QDebug>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDE {
    SessionProcess::SessionProcess(QObject *parent) : QProcess(parent) {
    }

    void SessionProcess::setUser(const QString &user) {
        m_user = user;
    }

    void SessionProcess::setDir(const QString &dir) {
        m_dir = dir;
    }

    void SessionProcess::setUid(int uid) {
        m_uid = uid;
    }

    void SessionProcess::setGid(int gid) {
        m_gid = gid;
    }

    void SessionProcess::setupChildProcess() {
#if !TEST
        Authenticator *authenticator = qobject_cast<Authenticator *>(parent());

        if (initgroups(qPrintable(m_user), m_gid)) {
            qCritical() << " DAEMON: Failed to initialize user groups.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);
        }

        if (setgid(m_gid)) {
            qCritical() << " DAEMON: Failed to set group id.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);
        }

        if (setuid(m_uid)) {
            qCritical() << " DAEMON: Failed to set user id.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);

        }

        // add cookie
        authenticator->addCookie(QString("%1/.Xauthority").arg(m_dir));

        // change to user home dir
        chdir(qPrintable(m_dir));

        // copy environment to pam environment
        for (int i = 0; environ[i] != nullptr; ++i)
            authenticator->putenv(environ[i]);
#endif
    }
}
