/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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
#include "Cookie.h"

#include <QDebug>

#include <security/pam_appl.h>

#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <unistd.h>
#include <wait.h>

namespace SDE {
    typedef int (conv_func)(int, const struct pam_message **, struct pam_response **, void *);

    class AuthenticatorPrivate {
    public:
        const char *cookie { nullptr };

        QString service { "" };
        QString display { "" };
        QString username { "" };
        QString password { "" };

        struct pam_conv pamc;
        pam_handle_t *pamh { nullptr };
        int pam_err { PAM_SUCCESS };
    };

    int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
        struct pam_response *aresp;

        // check size of the message buffer
        if ((n <= 0) || (n > PAM_MAX_NUM_MSG))
            return PAM_CONV_ERR;

        // create response buffer
        if ((aresp = (pam_response *) calloc(n, sizeof(struct pam_response))) == nullptr)
            return PAM_BUF_ERR;

        // respond to the messages
        bool failed = false;
        for (int i = 0; i < n; ++i) {
            aresp[i].resp_retcode = 0;
            aresp[i].resp = nullptr;
            switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
                // set password
                aresp[i].resp = strdup(((AuthenticatorPrivate *) data)->password.toStdString().c_str());
                if (aresp[i].resp == nullptr)
                    failed = true;
                break;
            case PAM_PROMPT_ECHO_ON:
                // set username
                aresp[i].resp = strdup(((AuthenticatorPrivate *) data)->username.toStdString().c_str());
                if (aresp[i].resp == nullptr)
                    failed = true;
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
            memset(aresp, 0, n * sizeof * aresp);
            *resp = nullptr;
            return PAM_CONV_ERR;
        }

        *resp = aresp;
        return PAM_SUCCESS;
    }

    Authenticator::Authenticator(const QString &service) : d(new AuthenticatorPrivate()) {
        d->service = service;

        // initialize pam
        d->pamc.conv = &converse;
        d->pamc.appdata_ptr = d;

        // start pam transaction
        pam_start(d->service.toStdString().c_str(), nullptr, &d->pamc, &d->pamh);
    }

    Authenticator::~Authenticator() {
        pam_end(d->pamh, d->pam_err);
    }

    void Authenticator::setCookie(const char *cookie) {
        d->cookie = cookie;
    }

    void Authenticator::setDisplay(const QString &display) {
        d->display = display;
    }

    void Authenticator::setUsername(const QString &username) {
        d->username = username;
    }

    void Authenticator::setPassword(const QString &password) {
        d->password = password;
    }

    const char *join(const char *name, char seperator, const char *value) {
        int size = strlen(name) + strlen(value) + 2;
        // create string
        char *result = new char[size];
        memset(result , 0, size);
        // concat string
        sprintf(result, "%s%c%s", name, seperator, value);
        // return result
        return result;
    }

    bool Authenticator::authenticate() {
        // set username
        if ((d->pam_err = pam_set_item(d->pamh, PAM_USER, d->username.toStdString().c_str())) != PAM_SUCCESS)
            return false;

        // set tty
        const char *tty = ttyname(STDERR_FILENO);
        if ((d->pam_err = pam_set_item(d->pamh, PAM_TTY, tty)) != PAM_SUCCESS)
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

    bool Authenticator::login(const QString &loginCommand) {
        // set credentials
        if ((d->pam_err = pam_setcred(d->pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return false;

        // get tty name
        const char *tty = ttyname(0);
        if (!tty)
            tty = d->display.toStdString().c_str();
        // set tty name
        if ((d->pam_err = pam_set_item(d->pamh, PAM_TTY, tty)) != PAM_SUCCESS)
            return false;

        // open session
        if ((d->pam_err = pam_open_session(d->pamh, 0)) != PAM_SUCCESS)
            return false;

        // get mapped user name; PAM may have changed it
        char *user;
        if ((d->pam_err = pam_get_item(d->pamh, PAM_USER, (const void **)&user)) != PAM_SUCCESS)
            return false;

        struct passwd *pw;
        if ((pw = getpwnam(user)) == nullptr)
            return false;

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // start login process
        pid_t pid = fork();

        if (pid == 0) {
            // set user groups, group id and user id
            if ((initgroups(pw->pw_name, pw->pw_gid) != 0) || (setgid(pw->pw_gid) != 0) || (setuid(pw->pw_uid) != 0)) {
                qCritical() << "error: could not switch user id.";
                _exit(1);
            }
            // authority file path
            const char *xauthority = join(pw->pw_dir, '/', ".Xauthority");
            // remove authority file
            remove(xauthority);
            // add cookie
            Cookie::add(d->cookie, d->display, xauthority);
            // copy environment to pam environment
            for (int i = 0; environ[i] != NULL; ++i)
                pam_putenv(d->pamh, environ[i]);
            // set some more environment variables
            pam_putenv(d->pamh, join("HOME", '=', pw->pw_dir));
            pam_putenv(d->pamh, join("PWD", '=', pw->pw_dir));
            pam_putenv(d->pamh, join("SHELL", '=', pw->pw_shell));
            pam_putenv(d->pamh, join("USER", '=', pw->pw_name));
            pam_putenv(d->pamh, join("LOGNAME", '=', pw->pw_name));
            pam_putenv(d->pamh, join("DISPLAY", '=', d->display.toStdString().c_str()));
            pam_putenv(d->pamh, join("MAIL", '=', join(_PATH_MAILDIR, '/', pw->pw_name)));
            pam_putenv(d->pamh, join("XAUTHORITY", '=', xauthority));
            pam_putenv(d->pamh, join("PATH", '=', Configuration::instance()->defaultPath().toStdString().c_str()));
            // execute session start script
            QString sessionCommand = QString("%1 %2").arg(Configuration::instance()->sessionCommand()).arg(loginCommand);
            // change to the current dir
            chdir(pw->pw_dir);
            execle(pw->pw_shell, pw->pw_shell, "-c", sessionCommand.toStdString().c_str(), NULL, pam_getenvlist(d->pamh));
            // if we returned from exec, an error occured
            qCritical() << "error: could not execute login command.";
            // exit
            _exit(1);
        }

        // wait until login processs ends
        int status;
        while (wait(&status) != pid);

        // close session
        pam_close_session(d->pamh, 0);
        // delete creds
        pam_setcred(d->pamh, PAM_DELETE_CRED);

        // send SIGHUP to client group
        killpg(pid, SIGHUP);

        // send SIGTERM to client group
        // if error, send SIGKILL group
        if (killpg(pid, SIGTERM))
            killpg(pid, SIGKILL);

        return true;
    }
}
