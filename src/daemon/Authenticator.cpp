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

#include "Authenticator.h"

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "DisplayManager.h"
#include "Seat.h"
#include "Session.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

#ifdef USE_PAM
#include "PamService.h"
#else
#include <crypt.h>
#include <shadow.h>
#endif

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Authenticator::Authenticator(Display *parent) : QObject(parent), m_display(parent) {
    }

    Authenticator::~Authenticator() {
        stop();
    #ifdef USE_PAM
        delete m_pam;
    #endif
    }

    Display *Authenticator::display() const {
        return m_display;
    }

    bool Authenticator::start(const QString &user, const QString &session) {
        return doStart(user, QString(), session, true);
    }

    bool Authenticator::start(const QString &user, const QString &password, const QString &session) {
        return doStart(user, password, session, false);
    }

    bool Authenticator::doStart(const QString &user, const QString &password, const QString &session, bool passwordless) {
        // check flag
        if (m_started)
            return false;

        // convert session to command
        QString sessionName = "";
        QString xdgSessionName = "";
        QString command = "";

        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(daemonApp->configuration()->sessionsDir());

            // session file
            QFile file(dir.absoluteFilePath(session));

            // open file
            if (file.open(QIODevice::ReadOnly)) {

                // read line-by-line
                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine();

                    // line starting with Exec
                    if (line.startsWith("Exec="))
                        command = line.mid(5);

                    // Desktop names, change the separator
                    if (line.startsWith("DesktopNames=")) {
                        xdgSessionName = line.mid(13);
                        xdgSessionName.replace(';', ':');
                    }
                }

                // close file
                file.close();
            }

            // remove extension
            sessionName = session.left(session.lastIndexOf("."));
        } else {
            command = session;
            sessionName = session;
        }

        if (command.isEmpty()) {
            // log error
            qCritical() << "Failed to find command for session:" << session;

            // return fail
            return false;
        }

        // get display and display
        Seat *seat = m_display->seat();

#ifdef USE_PAM
        if (m_pam)
            delete m_pam;

        if (!passwordless)
            m_pam = new PamService("sddm", user, password, passwordless);
        else
            m_pam = new PamService("sddm-autologin", user, password, passwordless);

        if (!m_pam)
            return false;

        // setup pam service
        m_pam->setDisplay(m_display);
        m_pam->setSessionClass("user");
        m_pam->setSessionType("x11");
        m_pam->setSessionDesktop(xdgSessionName);

        // authenticate the applicant
        char *mapped;
        if (!m_pam->authenticate(&mapped))
            return false;
#else
        if (!passwordless) {
            // user name
            struct passwd *pw;
            if ((pw = getpwnam(qPrintable(user))) == nullptr) {
                // log error
                qCritical() << "Failed to get user entry.";

                // return fail
                return false;
            }

            struct spwd *sp;
            if ((sp = getspnam(pw->pw_name)) == nullptr) {
                // log error
                qCritical() << "Failed to get shadow entry.";

                // return fail
                return false;
            }

            // check if password is not empty
            if (sp->sp_pwdp && sp->sp_pwdp[0]) {

                // encrypt password
                char *encrypted = crypt(qPrintable(password), sp->sp_pwdp);

                if (strcmp(encrypted, sp->sp_pwdp))
                    return false;
            }
        }

        char *mapped = strdup(qPrintable(user));
#endif

        // user name
        struct passwd *pw;
        if ((pw = getpwnam(mapped)) == nullptr) {
            // log error
            qCritical() << "Failed to get user name.";

            // return fail
            return false;
        }

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // create user session process
        process = new Session(QString("Session%1").arg(daemonApp->newSessionId()), m_display, this);

        // set session process params
        process->setUser(pw->pw_name);
        process->setDir(pw->pw_dir);
        process->setUid(pw->pw_uid);
        process->setGid(pw->pw_gid);

        // set process environment
#ifdef USE_PAM
        QProcessEnvironment env = m_pam->systemEnvironment();
#else
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        // we strdup'd the string before in this branch
        free(mapped);

        // session information
        env.insert("XDG_SESSION_CLASS", "user");
        env.insert("XDG_SESSION_TYPE", "x11");
        env.insert("XDG_SESSION_DESKTOP", xdgSessionName);
        env.insert("XDG_SEAT", seat->name());
        env.insert("XDG_VTNR", QString::number(m_display->terminalId()));
#endif
        env.insert("HOME", pw->pw_dir);
        env.insert("PWD", pw->pw_dir);
        env.insert("SHELL", pw->pw_shell);
        env.insert("USER", pw->pw_name);
        env.insert("LOGNAME", pw->pw_name);
        env.insert("PATH", daemonApp->configuration()->defaultPath());
        env.insert("DISPLAY", m_display->name());
        env.insert("XAUTHORITY", QString("%1/.Xauthority").arg(pw->pw_dir));
        env.insert("XDG_CURRENT_DESKTOP", xdgSessionName);
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(process->name()));
        env.insert("DESKTOP_SESSION", sessionName);
        process->setProcessEnvironment(env);

        // redirect error output to ~/.xession-errors
        process->setStandardErrorFile(QString("%1/.xsession-errors").arg(pw->pw_dir));

        // start session
        process->start(daemonApp->configuration()->sessionCommand(), { command });

        // connect signal
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        // wait for started
        if (!process->waitForStarted()) {
            // log error
            qDebug() << "Failed to start user session.";

            // return fail
            return false;
        }

        // log message
        qDebug() << "User session started.";

        // register to the display manager
        daemonApp->displayManager()->AddSession(process->name(), seat->name(), pw->pw_name);

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void Authenticator::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << "User session stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
        process->deleteLater();
        process = nullptr;
    }

    void Authenticator::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << "User session ended.";

        // unregister from the display manager
        daemonApp->displayManager()->RemoveSession(process->name());

        // delete session process
        process->deleteLater();
        process = nullptr;

#ifdef USE_PAM
        if (m_pam) {
            delete m_pam;
            m_pam = nullptr;
        }
#endif

        // emit signal
        emit stopped();
    }
}
