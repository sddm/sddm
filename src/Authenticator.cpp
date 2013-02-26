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
#include "Cookie.h"
#include "Util.h"

#include <QDebug>

#include <security/pam_appl.h>

#include <grp.h>
#include <paths.h>
#include <pwd.h>

namespace SDE {
    typedef int (conv_func)(int, const struct pam_message **, struct pam_response **, void *);

    class AuthenticatorPrivate {
    public:
        QString cookie { "" };

        QString service { "" };
        QString display { "" };
        QString username { "" };
        QString password { "" };

        struct pam_conv pamc;
        pam_handle_t *pamh { nullptr };
        int pam_err { PAM_SUCCESS };

        pid_t pid { -1 };
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
            memset(aresp, 0, n * sizeof(struct pam_response));
            free(aresp);
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
        // clean up
        delete d;
    }

    void Authenticator::setCookie(const QString &cookie) {
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

    void Authenticator::startSession(const QString &loginCommand) {
        // set credentials
        if ((d->pam_err = pam_setcred(d->pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return;

        // get tty name
        const char *tty = ttyname(0);
        if (!tty)
            tty = d->display.toStdString().c_str();
        // set tty name
        if ((d->pam_err = pam_set_item(d->pamh, PAM_TTY, tty)) != PAM_SUCCESS)
            return;

        // open session
        if ((d->pam_err = pam_open_session(d->pamh, 0)) != PAM_SUCCESS)
            return;

        // get mapped user name; PAM may have changed it
        char *user;
        if ((d->pam_err = pam_get_item(d->pamh, PAM_USER, (const void **)&user)) != PAM_SUCCESS)
            return;

        struct passwd *pw;
        if ((pw = getpwnam(user)) == nullptr)
            return;

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // execute session start script
        QString sessionCommand = QString("%1 %2").arg(Configuration::instance()->sessionCommand()).arg(loginCommand);
        d->pid = Util::execute(pw->pw_shell, QStringList() << "-c" << sessionCommand, [&] {
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
            for (int i = 0; environ[i] != nullptr; ++i)
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
            // change to the current dir
            int ignore_result = chdir(pw->pw_dir);
            static_cast<void>(ignore_result);
            // set environment
            char **envlist = pam_getenvlist(d->pamh);
            for (int i = 0; envlist[i] != nullptr; ++i)
                putenv(envlist[i]);
        });
    }

    void Authenticator::endSession() {
        if (d->pid > 0) {
            // wait until login processs ends
            Util::wait(d->pid);

            // terminate process group
            Util::terminate(d->pid);
        }

        // close session
        pam_close_session(d->pamh, 0);
        // delete creds
        pam_setcred(d->pamh, PAM_DELETE_CRED);
    }
}
