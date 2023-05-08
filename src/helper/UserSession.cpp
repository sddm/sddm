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

#include <QSocketNotifier>

#include "Configuration.h"
#include "Constants.h"
#include "UserSession.h"
#include "HelperApp.h"
#include "VirtualTerminal.h"
#include "XAuth.h"

#include <functional>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#ifdef Q_OS_FREEBSD
#include <login_cap.h>
#endif

namespace SDDM {
    UserSession::UserSession(HelperApp *parent)
        : QProcess(parent)
    {
        connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &UserSession::finished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        setChildProcessModifier(std::bind(&UserSession::childModifier, this));
#endif
    }

    bool UserSession::start() {
        auto helper = qobject_cast<HelperApp*>(parent());
        QProcessEnvironment env = processEnvironment();

        bool isWaylandGreeter = false;

        // If the Xorg display server was already started, write the passed
        // auth cookie to /tmp/xauth_XXXXXX. This is done in the parent process
        // so that it can clean up the file on session end.
        if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("x11")
            && m_displayServerCmd.isEmpty()) {
            // Create the Xauthority file
            QByteArray cookie = helper->cookie();
            if (cookie.isEmpty()) {
                qCritical() << "Can't start X11 session with empty auth cookie";
                return false;
            }

            // Place it into /tmp, which is guaranteed to be read/writeable by
            // everyone while having the sticky bit set to avoid messing with
            // other's files.
            m_xauthFile.setFileTemplate(QStringLiteral("/tmp/xauth_XXXXXX"));

            if (!m_xauthFile.open()) {
                qCritical() << "Could not create the Xauthority file";
                return false;
            }

            QString display = processEnvironment().value(QStringLiteral("DISPLAY"));

            if (!XAuth::writeCookieToFile(display, m_xauthFile.fileName(), cookie)) {
                qCritical() << "Failed to write the Xauthority file";
                m_xauthFile.close();
                return false;
            }

            env.insert(QStringLiteral("XAUTHORITY"), m_xauthFile.fileName());
            setProcessEnvironment(env);
        }

        if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("x11")) {
            QString command;
            if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
                command = m_path;
            } else {
                command = QStringLiteral("%1 \"%2\"").arg(mainConfig.X11.SessionCommand.get()).arg(m_path);
            }

            qInfo() << "Starting X11 session:" << m_displayServerCmd << command;
            if (m_displayServerCmd.isEmpty()) {
                auto args = QProcess::splitCommand(command);
                setProgram(args.takeFirst());
                setArguments(args);
            } else {
                setProgram(QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-helper-start-x11user"));
                setArguments({m_displayServerCmd, command});
            }
            QProcess::start();

        } else if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("wayland")) {
            if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
                Q_ASSERT(!m_displayServerCmd.isEmpty());
                setProgram(QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-helper-start-wayland"));
                setArguments({m_displayServerCmd, m_path});
                QProcess::start();
                isWaylandGreeter = true;
            } else {
                setProgram(mainConfig.Wayland.SessionCommand.get());
                setArguments(QStringList{m_path});
                qInfo() << "Starting Wayland user session:" << program() << m_path;
                QProcess::start();
                closeWriteChannel();
                closeReadChannel(QProcess::StandardOutput);
            }
        } else {
            qCritical() << "Unable to run user session: unknown session type";
        }

        const bool started = waitForStarted();
        m_cachedProcessId = processId();
        if (started) {
            return true;
        } else if (isWaylandGreeter) {
            // This is probably fine, we need the compositor to start first
            return true;
        }

        return false;
    }

    void UserSession::stop()
    {
        if (state() != QProcess::NotRunning) {
            terminate();
            const bool isGreeter = processEnvironment().value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter");

            // Wait longer for a session than a greeter
            if (!waitForFinished(isGreeter ? 5000 : 60000)) {
                kill();
                if (!waitForFinished(5000)) {
                    qWarning() << "Could not fully finish the process" << program();
                }
            }
        } else {
            Q_EMIT finished(Auth::HELPER_OTHER_ERROR);
        }
    }

    QString UserSession::displayServerCommand() const
    {
        return m_displayServerCmd;
    }

    void UserSession::setDisplayServerCommand(const QString &command)
    {
        m_displayServerCmd = command;
    }

    void UserSession::setPath(const QString& path) {
        m_path = path;
    }

    QString UserSession::path() const {
        return m_path;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void UserSession::childModifier() {
#else
    void UserSession::setupChildProcess() {
#endif
        // Session type
        QString sessionType = processEnvironment().value(QStringLiteral("XDG_SESSION_TYPE"));
        QString sessionClass = processEnvironment().value(QStringLiteral("XDG_SESSION_CLASS"));
        const bool hasDisplayServer = !m_displayServerCmd.isEmpty();
        const bool x11UserSession = sessionType == QLatin1String("x11") && sessionClass == QLatin1String("user");
        const bool waylandUserSession = sessionType == QLatin1String("wayland") && sessionClass == QLatin1String("user");

        // When the display server is part of the session, we leak the VT into
        // the session as stdin so that it stays open without races
        if (hasDisplayServer || waylandUserSession) {
            // open VT and get the fd
            int vtNumber = processEnvironment().value(QStringLiteral("XDG_VTNR")).toInt();
            QString ttyString = VirtualTerminal::path(vtNumber);
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
                _exit(Auth::HELPER_OTHER_ERROR);
            }

            // take control of the tty
            if (takeControl) {
                if (ioctl(STDIN_FILENO, TIOCSCTTY) < 0) {
                    const auto error = strerror(errno);
                    qCritical().nospace() << "Failed to take control of " << ttyString << " (" << QFileInfo(ttyString).owner() << "): " << error;
                    _exit(Auth::HELPER_TTY_ERROR);
                }
            }

            VirtualTerminal::jumpToVt(vtNumber, x11UserSession);
        }

#ifdef Q_OS_LINUX
        // enter Linux namespaces
        for (const QString &ns: mainConfig.Namespaces.get()) {
            qInfo() << "Entering namespace" << ns;
            int fd = ::open(qPrintable(ns), O_RDONLY);
            if (fd < 0) {
                qCritical("open(%s) failed: %s", qPrintable(ns), strerror(errno));
                exit(Auth::HELPER_OTHER_ERROR);
            }
            if (setns(fd, 0) != 0) {
                qCritical("setns(open(%s), 0) failed: %s", qPrintable(ns), strerror(errno));
                exit(Auth::HELPER_OTHER_ERROR);
            }
            ::close(fd);
        }
#endif

        // switch user
        const QByteArray username = qobject_cast<HelperApp*>(parent())->user().toLocal8Bit();
        struct passwd pw;
        struct passwd *rpw;
        long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (bufsize == -1)
            bufsize = 16384;
        QScopedPointer<char, QScopedPointerPodDeleter> buffer(static_cast<char*>(malloc(bufsize)));
        if (buffer.isNull()) {
            qCritical() << "Could not allocate buffer of size" << bufsize;
            exit(Auth::HELPER_OTHER_ERROR);
        }
        int err = getpwnam_r(username.constData(), &pw, buffer.data(), bufsize, &rpw);
        if (rpw == NULL) {
            if (err == 0)
                qCritical() << "getpwnam_r(" << username << ") username not found!";
            else
                qCritical() << "getpwnam_r(" << username << ") failed with error: " << strerror(err);
            exit(Auth::HELPER_OTHER_ERROR);
        }

        const int xauthHandle = m_xauthFile.handle();
        if (xauthHandle != -1 && fchown(xauthHandle, pw.pw_uid, pw.pw_gid) != 0) {
            qCritical() << "fchown failed for" << m_xauthFile.fileName();
            exit(Auth::HELPER_OTHER_ERROR);
        }

#if defined(Q_OS_FREEBSD)
        // execve() uses the environment prepared in Backend::openSession(),
        // therefore environment variables which are set here are ignored.
        if (setusercontext(NULL, &pw, pw.pw_uid, LOGIN_SETALL) != 0) {
            qCritical() << "setusercontext(NULL, *, " << pw.pw_uid << ", LOGIN_SETALL) failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }
#else
        if (setgid(pw.pw_gid) != 0) {
            qCritical() << "setgid(" << pw.pw_gid << ") failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }

        // fetch ambient groups from PAM's environment;
        // these are set by modules such as pam_groups.so
        int n_pam_groups = getgroups(0, NULL);
        gid_t *pam_groups = NULL;
        if (n_pam_groups > 0) {
            pam_groups = new gid_t[n_pam_groups];
            if ((n_pam_groups = getgroups(n_pam_groups, pam_groups)) == -1) {
                qCritical() << "getgroups() failed to fetch supplemental"
                            << "PAM groups for user:" << username;
                exit(Auth::HELPER_OTHER_ERROR);
            }
        } else {
            n_pam_groups = 0;
        }

        // fetch session's user's groups
        int n_user_groups = 0;
        gid_t *user_groups = NULL;
        if (-1 == getgrouplist(pw.pw_name, pw.pw_gid,
                               NULL, &n_user_groups)) {
            user_groups = new gid_t[n_user_groups];
            if ((n_user_groups = getgrouplist(pw.pw_name,
                                              pw.pw_gid, user_groups,
                                              &n_user_groups)) == -1 ) {
                qCritical() << "getgrouplist(" << pw.pw_name << ", " << pw.pw_gid
                            << ") failed";
                exit(Auth::HELPER_OTHER_ERROR);
            }
        }

        // set groups to concatenation of PAM's ambient
        // groups and the session's user's groups
        int n_groups = n_pam_groups + n_user_groups;
        if (n_groups > 0) {
            gid_t *groups = new gid_t[n_groups];
            memcpy(groups, pam_groups, (n_pam_groups * sizeof(gid_t)));
            memcpy((groups + n_pam_groups), user_groups,
                   (n_user_groups * sizeof(gid_t)));

            // setgroups(2) handles duplicate groups
            if (setgroups(n_groups, groups) != 0) {
                qCritical() << "setgroups() failed for user: " << username;
                exit (Auth::HELPER_OTHER_ERROR);
            }
            delete[] groups;
        }
        delete[] pam_groups;
        delete[] user_groups;

        if (setuid(pw.pw_uid) != 0) {
            qCritical() << "setuid(" << pw.pw_uid << ") failed for user: " << username;
            exit(Auth::HELPER_OTHER_ERROR);
        }
#endif /* Q_OS_FREEBSD */
        if (chdir(pw.pw_dir) != 0) {
            qCritical() << "chdir(" << pw.pw_dir << ") failed for user: " << username;
            qCritical() << "verify directory exist and has sufficient permissions";
            exit(Auth::HELPER_OTHER_ERROR);
        }

        if (sessionClass != QLatin1String("greeter")) {
            //we cannot use setStandardError file as this code is run in the child process
            //we want to redirect after we setuid so that the log file is owned by the user

            // determine stderr log file based on session type
            QString sessionLog = QStringLiteral("%1/%2")
                    .arg(QString::fromLocal8Bit(pw.pw_dir))
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
        }
    }

    qint64 UserSession::cachedProcessId() {
        return m_cachedProcessId;
    }

}
