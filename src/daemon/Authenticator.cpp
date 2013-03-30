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

#include <security/pam_appl.h>

#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    class AuthenticatorPrivate {
    public:
        struct pam_conv pamc;
        pam_handle_t *pamh { nullptr };
        int pam_err { PAM_SUCCESS };
    };

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

    Authenticator::Authenticator(QObject *parent) : QObject(parent), credentials(new Credentials(this)), d(new AuthenticatorPrivate()) {
        // initialize pam
        d->pamc = { &converse, credentials };

        // start pam service
        pam_start("sddm", nullptr, &d->pamc, &d->pamh);
    }

    Authenticator::~Authenticator() {
        stop();

        pam_end(d->pamh, d->pam_err);

        delete d;
    }

    void Authenticator::setDisplay(const QString &display) {
        m_display = display;
    }

    void Authenticator::putenv(const QString &value) {
        pam_putenv(d->pamh, qPrintable(value));
    }

    bool Authenticator::authenticate(const QString &user, const QString &password) {
        // set user name and password
        credentials->user = user;
        credentials->password = password;

        // set username
        if ((d->pam_err = pam_set_item(d->pamh, PAM_USER, qPrintable(credentials->user))) != PAM_SUCCESS)
            return false;

        // set tty
        if ((d->pam_err = pam_set_item(d->pamh, PAM_TTY, qPrintable(m_display))) != PAM_SUCCESS)
            return false;

        // set display name
        if ((d->pam_err = pam_set_item(d->pamh, PAM_XDISPLAY, qPrintable(m_display))) != PAM_SUCCESS)
            return false;

        // authenticate the applicant
        if ((d->pam_err = pam_authenticate(d->pamh, 0)) != PAM_SUCCESS)
            return false;

        if ((d->pam_err = pam_acct_mgmt(d->pamh, 0)) == PAM_NEW_AUTHTOK_REQD)
            d->pam_err = pam_chauthtok(d->pamh, PAM_CHANGE_EXPIRED_AUTHTOK);

        if (d->pam_err != PAM_SUCCESS)
            return false;

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

        // set credentials
        if ((d->pam_err = pam_setcred(d->pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return false;

        // set tty name
        if ((d->pam_err = pam_set_item(d->pamh, PAM_TTY, qPrintable(m_display))) != PAM_SUCCESS)
            return false;

        // open session
        if ((d->pam_err = pam_open_session(d->pamh, 0)) != PAM_SUCCESS)
            return false;

        // get mapped user name; PAM may have changed it
        char *pamUser;
        if ((d->pam_err = pam_get_item(d->pamh, PAM_USER, (const void **)&pamUser)) != PAM_SUCCESS)
            return false;

        // user name
        struct passwd *pw;
        if ((pw = getpwnam(pamUser)) == nullptr) {
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

        // get parent
        Display *display = qobject_cast<Display *>(parent());
        Seat *seat = qobject_cast<Seat *>(display->parent());

        // create user session process
        process = new Session(QString("Session%1").arg(daemonApp->newSessionId()), this);

        // set session process params
        process->setUser(pw->pw_name);
        process->setDir(pw->pw_dir);
        process->setUid(pw->pw_uid);
        process->setGid(pw->pw_gid);

        // set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("HOME", pw->pw_dir);
        env.insert("PWD", pw->pw_dir);
        env.insert("SHELL", pw->pw_shell);
        env.insert("USER", pw->pw_name);
        env.insert("LOGNAME", pw->pw_name);
        env.insert("MAIL", QString("%1/%2").arg(_PATH_MAILDIR).arg(pw->pw_name));
        env.insert("PATH", Configuration::instance()->defaultPath());
        env.insert("DISPLAY", m_display);
        env.insert("XAUTHORITY", QString("%1/.Xauthority").arg(pw->pw_dir));
        env.insert("XDG_SEAT", seat->name());
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(process->name()));
        env.insert("XDG_VTNR", QString::number(display->terminalId()));
        env.insert("DESKTOP_SESSION", sessionName);
        env.insert("GDMSESSION", sessionName);
        process->setProcessEnvironment(env);

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

        // close session
        pam_close_session(d->pamh, 0);

        // delete creds
        pam_setcred(d->pamh, PAM_DELETE_CRED);

        // emit signal
        emit stopped();
    }
}
