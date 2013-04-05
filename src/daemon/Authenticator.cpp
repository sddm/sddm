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

#if PAM_FOUND
#include <security/pam_appl.h>
#else
#include <crypt.h>
#include <shadow.h>
#endif

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
#if PAM_FOUND
    typedef int (conv_func)(int, const struct pam_message **, struct pam_response **, void *);

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
                Credentials *c = static_cast<Credentials *>(data);
                // set password
                aresp[i].resp = strdup(qPrintable(c->password));
                if (aresp[i].resp == nullptr)
                    failed = true;
                // clear password
                c->password = "";
            }
                break;
            case PAM_PROMPT_ECHO_ON: {
                Credentials *c = static_cast<Credentials *>(data);
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

    class PamService {
    public:
        PamService(const char *service, void *data) {
            // create context
            m_converse = { &converse, data };

            // start service
            pam_start(service, nullptr, &m_converse, &handle);
        }

        ~PamService() {
            // stop service
            pam_end(handle, result);
        }

        pam_handle_t *handle { nullptr };
        int result { PAM_SUCCESS };

    private:
        struct pam_conv m_converse;
    };

#endif

    Authenticator::Authenticator(QObject *parent) : QObject(parent), credentials(new Credentials(this)) {
    }

    Authenticator::~Authenticator() {
        stop();
    }

    bool Authenticator::authenticate(const QString &user, const QString &password) {
        // set user name and password
        credentials->user = user;
        credentials->password = password;

#if PAM_FOUND
        PamService pam("sddm", credentials);
        Display *display = qobject_cast<Display *>(parent());

        // set username
        if ((pam.result = pam_set_item(pam.handle, PAM_USER, qPrintable(credentials->user))) != PAM_SUCCESS)
            return false;

        // set tty
        if ((pam.result = pam_set_item(pam.handle, PAM_TTY, qPrintable(display->name()))) != PAM_SUCCESS)
            return false;

        // set display name
        if ((pam.result = pam_set_item(pam.handle, PAM_XDISPLAY, qPrintable(display->name()))) != PAM_SUCCESS)
            return false;

        // authenticate the applicant
        if ((pam.result = pam_authenticate(pam.handle, 0)) != PAM_SUCCESS)
            return false;

        if ((pam.result = pam_acct_mgmt(pam.handle, 0)) == PAM_NEW_AUTHTOK_REQD)
            pam.result = pam_chauthtok(pam.handle, PAM_CHANGE_EXPIRED_AUTHTOK);

        if (pam.result != PAM_SUCCESS)
            return false;
#else

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

        // check if pass is empty
        if (sp->sp_pwdp == 0 || sp->sp_pwdp[0] == '\0')
            return true;

        // encryp password
        char *encrypted = crypt(qPrintable(password), sp->sp_pwdp);

        // check and return result
        return (strcmp(encrypted, sp->sp_pwdp) == 0);
#endif

        return true;
    }

    bool Authenticator::start(const QString &user, const QString &session) {
        // check flag
        if (m_started)
            return false;

        // set user name
        credentials->user = user;

        // convert session to command
        QString sessionName = "";
        QString command = "";

        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(Configuration::instance()->sessionsDir());

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
        Display *display = qobject_cast<Display *>(parent());
        Seat *seat = qobject_cast<Seat *>(display->parent());

#if PAM_FOUND
        PamService pam("sddm", credentials);

        // set credentials
        if ((pam.result = pam_setcred(pam.handle, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return false;

        // set tty name
        if ((pam.result = pam_set_item(pam.handle, PAM_TTY, qPrintable(display->name()))) != PAM_SUCCESS)
            return false;

        // open session
        if ((pam.result = pam_open_session(pam.handle, 0)) != PAM_SUCCESS)
            return false;

        // get mapped user name; PAM may have changed it
        char *mapped;
        if ((pam.result = pam_get_item(pam.handle, PAM_USER, (const void **)&mapped)) != PAM_SUCCESS)
            return false;
#else
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
#if PAM_FOUND
        // get pam environment
        char **envlist = pam_getenvlist(pam.handle);

        // copy it to the env map
        for (int i = 0; envlist[i] != nullptr; ++i) {
            QString s(envlist[i]);

            // find equal sign
            int index = s.indexOf('=');

            // add to the hash
            if (index != -1)
                env.insert(s.left(index), s.mid(index + 1));
        }
#endif
        env.insert("HOME", pw->pw_dir);
        env.insert("PWD", pw->pw_dir);
        env.insert("SHELL", pw->pw_shell);
        env.insert("USER", pw->pw_name);
        env.insert("LOGNAME", pw->pw_name);
        env.insert("PATH", Configuration::instance()->defaultPath());
        env.insert("DISPLAY", display->name());
        env.insert("XAUTHORITY", QString("%1/.Xauthority").arg(pw->pw_dir));
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(process->name()));
        env.insert("DESKTOP_SESSION", sessionName);
        env.insert("GDMSESSION", sessionName);
        process->setProcessEnvironment(env);

        // redirect error output to ~/.xession-errors
        process->setStandardErrorFile(QString("%1/.xsession-errors").arg(pw->pw_dir));

        // start session
        process->start(Configuration::instance()->sessionCommand(), { command });

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

        // emit signal
        emit stopped();
    }
}
