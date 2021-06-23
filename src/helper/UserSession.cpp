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
#include "UserSession.h"
#include "HelperApp.h"
#include "VirtualTerminal.h"
#include "XAuth.h"
#include "xorguserhelper.h"
#include "waylandhelper.h"

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

namespace SDDM {
    UserSession::UserSession(HelperApp *parent)
        : QObject(parent)
        , m_process(new QProcess(this))
        , m_xorgUser(new XOrgUserHelper(this))
    {
        m_process->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &UserSession::finished);
        connect(m_xorgUser, &XOrgUserHelper::displayChanged, this, [this, parent](const QString &display) {
            auto env = processEnvironment();
            env.insert(QStringLiteral("DISPLAY"), m_xorgUser->display());
            env.insert(QStringLiteral("XAUTHORITY"), m_xorgUser->xauthPath());
            setProcessEnvironment(env);

            parent->displayServerStarted(display);
        });
    }

    bool UserSession::start() {
        auto helper = qobject_cast<HelperApp*>(parent());
        QProcessEnvironment env = helper->session()->processEnvironment();

        setup();

        if (!m_displayServerCmd.isEmpty()) {
            if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("wayland") && env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
                m_wayland = new WaylandHelper(this);
                m_wayland->setEnvironment(env);
                if (!m_wayland->startCompositor(m_displayServerCmd))
                    return false;
            } else {
                m_xorgUser->setEnvironment(env);
                if (!m_xorgUser->start(m_displayServerCmd))
                    return false;
            }
        }

        bool isWaylandGreeter = false;
        if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("x11")) {
            if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
                qInfo() << "Starting X11 greeter session:" << m_path;
                auto args = QProcess::splitCommand(m_path);
                const auto program = args.takeFirst();
                m_process->start(program, args);
            } else {
                const QString cmd = QStringLiteral("%1 \"%2\"").arg(mainConfig.X11.SessionCommand.get()).arg(m_path);
                qInfo() << "Starting X11 user session:" << cmd;
                m_process->start(mainConfig.X11.SessionCommand.get(), QStringList{m_path});
            }
        } else if (env.value(QStringLiteral("XDG_SESSION_TYPE")) == QLatin1String("wayland")) {
            if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
                isWaylandGreeter = true;
                auto args = QProcess::splitCommand(m_path);
                m_process->setProgram(args.takeFirst());
                m_process->setArguments(args);
                m_wayland->startGreeter(m_process);
            } else {
                const QString cmd = QStringLiteral("%1 %2").arg(mainConfig.Wayland.SessionCommand.get()).arg(m_path);
                qInfo() << "Starting Wayland user session:" << cmd;
                m_process->start(mainConfig.Wayland.SessionCommand.get(), QStringList{m_path});
                m_process->closeWriteChannel();
                m_process->closeReadChannel(QProcess::StandardOutput);
            }
        } else {
            qCritical() << "Unable to run user session: unknown session type";
        }

        if (m_process->waitForStarted()) {
            int vtNumber = processEnvironment().value(QStringLiteral("XDG_VTNR")).toInt();
            VirtualTerminal::jumpToVt(vtNumber, true);
            return true;
        } else if (isWaylandGreeter) {
            // This is probably fine, we need the compositor to start first
            return true;
        }

        return false;
    }

    void UserSession::stop()
    {
        m_process->terminate();
        if (!m_process->waitForFinished(5000))
            m_process->kill();

        if (!m_displayServerCmd.isEmpty()) {
            m_xorgUser->stop();
            m_wayland->stop();
        }
    }

    void UserSession::terminate()
    {
        m_process->terminate();
    }

    QProcessEnvironment UserSession::processEnvironment() const
    {
        return m_process->processEnvironment();
    }

    void UserSession::setProcessEnvironment(const QProcessEnvironment &env)
    {
        m_process->setProcessEnvironment(env);
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

    qint64 UserSession::processId() const
    {
        return m_process->processId();
    }

    void UserSession::setup() {
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
            QString ttyString = QStringLiteral("/dev/tty%1").arg(vtNumber);
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
                exit(AuthEnums::HELPER_OTHER_ERROR);
            }

            // take control of the tty
            if (takeControl) {
                if (ioctl(STDIN_FILENO, TIOCSCTTY) < 0) {
                    qCritical("Failed to take control of the tty: %s", strerror(errno));
                    exit(AuthEnums::HELPER_OTHER_ERROR);
                }
            }

            VirtualTerminal::jumpToVt(vtNumber, false);
        }

#ifdef Q_OS_LINUX
        // enter Linux namespaces
        for (const QString &ns: mainConfig.Namespaces.get()) {
            qInfo() << "Entering namespace" << ns;
            int fd = ::open(qPrintable(ns), O_RDONLY);
            if (fd < 0) {
                qCritical("open(%s) failed: %s", qPrintable(ns), strerror(errno));
                exit(AuthEnums::HELPER_OTHER_ERROR);
            }
            if (setns(fd, 0) != 0) {
                qCritical("setns(open(%s), 0) failed: %s", qPrintable(ns), strerror(errno));
                exit(AuthEnums::HELPER_OTHER_ERROR);
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
        if (buffer.isNull())
            exit(AuthEnums::HELPER_OTHER_ERROR);
        int err = getpwnam_r(username.constData(), &pw, buffer.data(), bufsize, &rpw);
        if (rpw == NULL) {
            if (err == 0)
                qCritical() << "getpwnam_r(" << username << ") username not found!";
            else
                qCritical() << "getpwnam_r(" << username << ") failed with error: " << strerror(err);
            exit(AuthEnums::HELPER_OTHER_ERROR);
        }
        if (setgid(pw.pw_gid) != 0) {
            qCritical() << "setgid(" << pw.pw_gid << ") failed for user: " << username;
            exit(AuthEnums::HELPER_OTHER_ERROR);
        }
        qputenv("XDG_RUNTIME_DIR", QByteArrayLiteral("/run/user/") + QByteArray::number(pw.pw_uid));

#ifdef USE_PAM

        // fetch ambient groups from PAM's environment;
        // these are set by modules such as pam_groups.so
        int n_pam_groups = getgroups(0, NULL);
        gid_t *pam_groups = NULL;
        if (n_pam_groups > 0) {
            pam_groups = new gid_t[n_pam_groups];
            if ((n_pam_groups = getgroups(n_pam_groups, pam_groups)) == -1) {
                qCritical() << "getgroups() failed to fetch supplemental"
                            << "PAM groups for user:" << username;
                exit(AuthEnums::HELPER_OTHER_ERROR);
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
                exit(AuthEnums::HELPER_OTHER_ERROR);
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
                exit (AuthEnums::HELPER_OTHER_ERROR);
            }
            delete[] groups;
        }
        delete[] pam_groups;
        delete[] user_groups;

#else

        if (initgroups(pw.pw_name, pw.pw_gid) != 0) {
            qCritical() << "initgroups(" << pw.pw_name << ", " << pw.pw_gid << ") failed for user: " << username;
            exit(AuthEnums::HELPER_OTHER_ERROR);
        }

#endif /* USE_PAM */

        if (setuid(pw.pw_uid) != 0) {
            qCritical() << "setuid(" << pw.pw_uid << ") failed for user: " << username;
            exit(AuthEnums::HELPER_OTHER_ERROR);
        }
        if (chdir(pw.pw_dir) != 0) {
            qCritical() << "chdir(" << pw.pw_dir << ") failed for user: " << username;
            qCritical() << "verify directory exist and has sufficient permissions";
            exit(AuthEnums::HELPER_OTHER_ERROR);
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

            m_process->setStandardErrorFile(sessionLog);
        }

        // set X authority for X11 sessions only
        if (x11UserSession) {
            QString cookie = qobject_cast<HelperApp*>(parent())->cookie();
            if (!cookie.isEmpty()) {
                QString file = processEnvironment().value(QStringLiteral("XAUTHORITY"));
                QString display = processEnvironment().value(QStringLiteral("DISPLAY"));

                // Create the path
                QFileInfo finfo(file);
                QDir().mkpath(finfo.absolutePath());

                XAuth::addCookieToFile(display, file, cookie);
            }
        }
    }
}
