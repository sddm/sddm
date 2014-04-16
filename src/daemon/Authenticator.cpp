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
#include <security/pam_appl.h>
#else
#include <crypt.h>
#include <shadow.h>
#endif

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
#ifdef USE_PAM
    class PamService {
    public:
        PamService(const char *service, const QString &user, const QString &password, bool passwordless);
        ~PamService();

        struct pam_conv m_converse;
        pam_handle_t *handle { nullptr };
        int result { PAM_SUCCESS };

        QString user { "" };
        QString password { "" };
        bool passwordless { false };
    };

    int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
        struct pam_response *aresp;

        // check size of the message buffer
        if ((n <= 0) || (n > PAM_MAX_NUM_MSG))
            return PAM_CONV_ERR;

        // create response buffer
        if ((aresp = (struct pam_response *) calloc(n, sizeof(struct pam_response))) == nullptr)
            return PAM_BUF_ERR;

        // respond to the messages
        bool failed = false;
        for (int i = 0; i < n; ++i) {
            aresp[i].resp_retcode = 0;
            aresp[i].resp = nullptr;
            switch (msg[i]->msg_style) {
                case PAM_PROMPT_ECHO_OFF: {
                    PamService *c = static_cast<PamService *>(data);
                    // set password
                    aresp[i].resp = strdup(qPrintable(c->password));
                    if (aresp[i].resp == nullptr)
                        failed = true;
                    // clear password
                    c->password = "";
                }
                    break;
                case PAM_PROMPT_ECHO_ON: {
                    PamService *c = static_cast<PamService *>(data);
                    // set user
                    aresp[i].resp = strdup(qPrintable(c->user));
                    if (aresp[i].resp == nullptr)
                        failed = true;
                    // clear user
                    c->user = "";
                }
                    break;
                case PAM_ERROR_MSG:
                case PAM_TEXT_INFO:
                    break;
                default:
                    failed = true;
            }
        }

        if (failed) {
            for (int i = 0; i < n; ++i) {
                if (aresp[i].resp != nullptr) {
                    memset(aresp[i].resp, 0, strlen(aresp[i].resp));
                    free(aresp[i].resp);
                }
            }
            memset(aresp, 0, n * sizeof(struct pam_response));
            free(aresp);
            *resp = nullptr;
            return PAM_CONV_ERR;
        }

        *resp = aresp;
        return PAM_SUCCESS;
    }

    PamService::PamService(const char *service, const QString &user, const QString &password, bool passwordless) : user(user), password(password), passwordless(passwordless) {
        // create context
        m_converse = { &converse, this };

        // start service
        pam_start(service, nullptr, &m_converse, &handle);
    }

    PamService::~PamService() {
        // stop service
        pam_end(handle, result);
    }
#endif

    Authenticator::Authenticator(Display *parent) : QObject(parent), m_display(parent) {
    }

    Authenticator::~Authenticator() {
        stop();
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
            qCritical() << " DAEMON: Failed to find command for session:" << session;

            // return fail
            return false;
        }

        // get display and display
        Seat *seat = m_display->seat();

#ifdef USE_PAM
        if (m_pam)
            delete m_pam;

        m_pam = new PamService("sddm", user, password, passwordless);

        if (!m_pam)
            return false;

        if (!passwordless) {
            // authenticate the applicant
            if ((m_pam->result = pam_authenticate(m_pam->handle, 0)) != PAM_SUCCESS)
                return false;

            if ((m_pam->result = pam_acct_mgmt(m_pam->handle, 0)) == PAM_NEW_AUTHTOK_REQD)
                m_pam->result = pam_chauthtok(m_pam->handle, PAM_CHANGE_EXPIRED_AUTHTOK);

            if (m_pam->result != PAM_SUCCESS)
                return false;
        }

        // set username
        if ((m_pam->result = pam_set_item(m_pam->handle, PAM_USER, qPrintable(user))) != PAM_SUCCESS)
            return false;

        // set credentials
        if ((m_pam->result = pam_setcred(m_pam->handle, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return false;

        // set tty
        if ((m_pam->result = pam_set_item(m_pam->handle, PAM_TTY, qPrintable(m_display->name()))) != PAM_SUCCESS)
            return false;

        // set display name
        if ((m_pam->result = pam_set_item(m_pam->handle, PAM_XDISPLAY, qPrintable(m_display->name()))) != PAM_SUCCESS)
            return false;

        // open session
        if ((m_pam->result = pam_open_session(m_pam->handle, 0)) != PAM_SUCCESS)
            return false;

        // get mapped user name; PAM may have changed it
        char *mapped;
        if ((m_pam->result = pam_get_item(m_pam->handle, PAM_USER, (const void **)&mapped)) != PAM_SUCCESS)
            return false;
#else
        if (!passwordless) {
            // user name
            struct passwd *pw;
            if ((pw = getpwnam(qPrintable(user))) == nullptr) {
                // log error
                qCritical() << " DAEMON: Failed to get user entry.";

                // return fail
                return false;
            }

            struct spwd *sp;
            if ((sp = getspnam(pw->pw_name)) == nullptr) {
                // log error
                qCritical() << " DAEMON: Failed to get shadow entry.";

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
            qCritical() << " DAEMON: Failed to get user name.";

            // return fail
            return false;
        }

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // create user session process
        process = new Session(QString("Session%1").arg(daemonApp->newSessionId()), this);

        // set session process params
        process->setUser(pw->pw_name);
        process->setDir(pw->pw_dir);
        process->setUid(pw->pw_uid);
        process->setGid(pw->pw_gid);

        // set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef USE_PAM
        // get pam environment
        char **envlist = pam_getenvlist(m_pam->handle);

        // copy it to the env map
        for (int i = 0; envlist[i] != nullptr; ++i) {
            QString s(envlist[i]);

            // find equal sign
            int index = s.indexOf('=');

            // add to the hash
            if (index != -1)
                env.insert(s.left(index), s.mid(index + 1));

            free(envlist[i]);
        }
        free(envlist);
#else
        // we strdup'd the string before in this branch
        free(mapped);
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
        env.insert("XDG_SEAT", seat->name());
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(process->name()));
        env.insert("XDG_VTNR", QString::number(m_display->terminalId()));
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
            qDebug() << " DAEMON: Failed to start user session.";

            // return fail
            return false;
        }

        // log message
        qDebug() << " DAEMON: User session started.";

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
        qDebug() << " DAEMON: User session stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
    }

    void Authenticator::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " DAEMON: User session ended.";

        // unregister from the display manager
        daemonApp->displayManager()->RemoveSession(process->name());

        // delete session process
        process->deleteLater();
        process = nullptr;

#ifdef USE_PAM
        if (m_pam) {
            m_pam->result = pam_close_session(m_pam->handle, 0);
            m_pam->result = pam_setcred(m_pam->handle, PAM_DELETE_CRED);
            delete m_pam;
            m_pam = nullptr;
        }
#endif

        // emit signal
        emit stopped();
    }
}
