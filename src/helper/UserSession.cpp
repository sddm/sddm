/*
 * Session process wrapper
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

#include "Configuration.h"
#include "UserSession.h"
#include "HelperApp.h"

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

namespace SDDM {
    UserSession::UserSession(HelperApp *parent)
            : QProcess(parent) {
    }

    UserSession::~UserSession() {

    }

    bool UserSession::start() {
        QProcessEnvironment env = qobject_cast<HelperApp*>(parent())->session()->processEnvironment();

        if (env.value("XDG_SESSION_CLASS") == "greeter")
            QProcess::start(m_path);
        else {
            qDebug() << "Starting:" << mainConfig.XDisplay.SessionCommand.get()
                     << m_path;
            QProcess::start(mainConfig.XDisplay.SessionCommand.get(),
                            QStringList() << m_path);
        }

        return waitForStarted();
    }

    void UserSession::setPath(const QString& path) {
        m_path = path;
    }

    QString UserSession::path() const {
        return m_path;
    }

    void UserSession::bail(int status) {
        emit finished(status, QProcess::NormalExit);
        exit(status);
    }

    void UserSession::setupChildProcess() {
        struct passwd *pw = getpwnam(qobject_cast<HelperApp*>(parent())->user().toLocal8Bit());
        if (setgid(pw->pw_gid) != 0)
            bail(2);
        if (initgroups(pw->pw_name, pw->pw_gid) != 0)
            bail(2);
        if (setuid(pw->pw_uid) != 0)
            bail(2);
        chdir(pw->pw_dir);
        // redirect standard error to a file
        setStandardErrorFile(QString("%1/.xsession-errors").arg(pw->pw_dir));

        QString cookie = qobject_cast<HelperApp*>(parent())->cookie();
        if (!cookie.isEmpty()) {
            QString file = processEnvironment().value("XAUTHORITY");
            QString display = processEnvironment().value("DISPLAY");
            qDebug() << "Adding cookie to" << file;

            QFile file_handler(file);
            file_handler.open(QIODevice::WriteOnly);
            file_handler.close();

            QString cmd = QString("%1 -f %2 -q").arg(mainConfig.XDisplay.XauthPath.get()).arg(file);

            // execute xauth
            FILE *fp = popen(qPrintable(cmd), "w");

            // check file
            if (!fp)
                return;
            fprintf(fp, "remove %s\n", qPrintable(display));
            fprintf(fp, "add %s . %s\n", qPrintable(display), qPrintable(cookie));
            fprintf(fp, "exit\n");

            // close pipe
            pclose(fp);
        }
    }
}
