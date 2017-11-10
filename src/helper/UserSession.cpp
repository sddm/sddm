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

#include "Configuration.h"
#include "UserSession.h"
#include "HelperApp.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <fcntl.h>

namespace SDDM {
    UserSession::UserSession(HelperApp *parent)
            : QProcess(parent) {
    }

    UserSession::~UserSession() {

    }

    bool UserSession::start() {
        QProcessEnvironment env = qobject_cast<HelperApp*>(parent())->session()->processEnvironment();

        if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
            QProcess::start(m_path);
        } else if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("x11")) {
            const QString cmd = QStringLiteral("%1 \"%2\"").arg(mainConfig.X11.SessionCommand.get()).arg(m_path);
            qInfo() << "Starting:" << cmd;
            QProcess::start(cmd);
        } else if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("wayland")) {
            const QString cmd = QStringLiteral("%1 %2").arg(mainConfig.Wayland.SessionCommand.get()).arg(m_path);
            qInfo() << "Starting:" << cmd;
            QProcess::start(cmd);
        } else {
            qCritical() << "Unable to run user session: unknown session type";
        }

        return waitForStarted();
    }

    void UserSession::setPath(const QString& path) {
        m_path = path;
    }

    QString UserSession::path() const {
        return m_path;
    }

    void UserSession::setupChildProcess() {
        // Session type
        QString sessionType = processEnvironment().value(QStringLiteral("XDG_SESSION_TYPE"));

        // For Wayland sessions we leak the VT into the session as stdin so
        // that it stays open without races
        if (sessionType == QLatin1String("wayland")) {
            // open VT and get the fd
            QString ttyString = QStringLiteral("/dev/tty%1").arg(processEnvironment().value(QStringLiteral("XDG_VTNR")));
            int vtFd = ::open(qPrintable(ttyString), O_RDWR | O_NOCTTY);

            // when this is true we'll take control of the tty
            bool takeControl = false;

            if (vtFd > 0) {
                dup2(vtFd, STDIN_FILENO);
                ::close(vtFd);
                takeControl = true;
            } else {
                int stdinFd = ::open("/dev/null", O_RDWR);
                dup2(stdinFd, STDIN_FILENO);
                ::close(stdinFd);
            }

            // set this process as session leader
            if (setsid() < 0) {
                qCritical("Failed to set pid %lld as leader of the new session and process group: %s",
                          QCoreApplication::applicationPid(), strerror(errno));
                exit(Auth::HELPER_OTHER_ERROR);
            }

            // take control of the tty
            if (takeControl) {
                if (ioctl(STDIN_FILENO, TIOCSCTTY) < 0) {
                    qCritical("Failed to take control of the tty: %s", strerror(errno));
                    exit(Auth::HELPER_OTHER_ERROR);
                }
            }
        }

        const QByteArray username = qobject_cast<HelperApp*>(parent())->user().toLocal8Bit();
        struct passwd *pw = getpwnam(username.constData());
        if (setgid(pw->pw_gid) != 0) {
            qCritical() << "setgid(" << pw->pw_gid << ") failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }
        if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
            qCritical() << "initgroups(" << pw->pw_name << ", " << pw->pw_gid << ") failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }
        if (setuid(pw->pw_uid) != 0) {
            qCritical() << "setuid(" << pw->pw_uid << ") failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }
        if (chdir(pw->pw_dir) != 0) {
            qCritical() << "chdir(" << pw->pw_dir << ") failed for user: " << username;
            qCritical() << "verify directory exist and has sufficient permissions";
            exit(Auth::HELPER_OTHER_ERROR);
        }

        //we cannot use setStandardError file as this code is run in the child process
        //we want to redirect after we setuid so that the log file is owned by the user

        // determine stderr log file based on session type
        QString sessionLog = QStringLiteral("%1/%2")
                .arg(QString::fromLocal8Bit(pw->pw_dir))
                .arg(sessionType == QLatin1String("x11")
                     ? mainConfig.X11.SessionLogFile.get()
                     : mainConfig.Wayland.SessionLogFile.get());

        // create the path
        QFileInfo finfo(sessionLog);
        QDir().mkpath(finfo.absolutePath());

        //swap the stderr pipe of this subprcess into a file
        int fd = ::open(qPrintable(sessionLog), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0)
        {
            dup2 (fd, STDERR_FILENO);
            ::close(fd);
        } else {
            qWarning() << "Could not open stderr to" << sessionLog;
        }

        //redirect any stdout to /dev/null
        fd = ::open("/dev/null", O_WRONLY);
        if (fd >= 0)
        {
            dup2 (fd, STDOUT_FILENO);
            ::close(fd);
        } else {
            qWarning() << "Could not redirect stdout";
        }

        // set X authority for X11 sessions only
        if (sessionType != QLatin1String("x11"))
            return;
        QString cookie = qobject_cast<HelperApp*>(parent())->cookie();
        if (!cookie.isEmpty()) {
            QString file = processEnvironment().value(QStringLiteral("XAUTHORITY"));
            QString display = processEnvironment().value(QStringLiteral("DISPLAY"));
            qDebug() << "Adding cookie to" << file;


            // create the path
            QFileInfo finfo(file);
            QDir().mkpath(finfo.absolutePath());

            QFile file_handler(file);
            file_handler.open(QIODevice::Append);
            file_handler.close();

            QString cmd = QStringLiteral("%1 -f %2 -q").arg(mainConfig.X11.XauthPath.get()).arg(file);

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

    void UserSession::setCachedProcessId(qint64 pid) {
        m_cachedProcessId = pid;
    }

    qint64 UserSession::cachedProcessId() {
        return m_cachedProcessId;
    }

}
