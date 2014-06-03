/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "PamService.h"

#include "Display.h"
#include "Seat.h"

namespace SDDM {
    static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
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
                    //c->user = "";
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
        pam_start(service, nullptr, &m_converse, &m_handle);
    }

    PamService::~PamService() {
        // stop service
        m_result = pam_close_session(m_handle, 0);
        m_result = pam_setcred(m_handle, PAM_DELETE_CRED);
        pam_end(m_handle, m_result);
    }

    Display *PamService::display() const {
        return m_display;
    }

    void PamService::setDisplay(Display *display) {
        m_display = display;
    }

    QString PamService::sessionClass() const {
        return m_sessionClass;
    }

    void PamService::setSessionClass(const QString &sessionClass) {
        m_sessionClass = sessionClass;
    }

    QString PamService::sessionType() const {
        return m_sessionType;
    }

    void PamService::setSessionType(const QString &sessionType) {
        m_sessionType = sessionType;
    }

    QString PamService::sessionDesktop() const {
        return m_sessionDesktop;
    }

    void PamService::setSessionDesktop(const QString &sessionDesktop) {
        m_sessionDesktop = sessionDesktop;
    }

    QProcessEnvironment PamService::systemEnvironment() const {
        // get system environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        // get pam environment
        char **envlist = pam_getenvlist(m_handle);

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

        return env;
    }

    bool PamService::authenticate(char **mapped) {
        // authenticate the applicant
        if ((m_result = pam_authenticate(m_handle, 0)) != PAM_SUCCESS)
            return false;

        if ((m_result = pam_acct_mgmt(m_handle, 0)) == PAM_NEW_AUTHTOK_REQD)
            m_result = pam_chauthtok(m_handle, PAM_CHANGE_EXPIRED_AUTHTOK);

        if (m_result != PAM_SUCCESS)
            return false;

        // set username
        if ((m_result = pam_set_item(m_handle, PAM_USER, qPrintable(user))) != PAM_SUCCESS)
            return false;

        // set credentials
        if ((m_result = pam_setcred(m_handle, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
            return false;

        if (m_display) {
            // set tty
            if ((m_result = pam_set_item(m_handle, PAM_TTY, qPrintable(m_display->name()))) != PAM_SUCCESS)
                return false;

            // set display name
            if ((m_result = pam_set_item(m_handle, PAM_XDISPLAY, qPrintable(m_display->name()))) != PAM_SUCCESS)
                return false;
        }

        // specify session information for logind
        if (!m_sessionClass.isEmpty()) {
            if ((m_result = pam_putenv(m_handle, qPrintable("XDG_SESSION_CLASS=" + m_sessionClass))) != PAM_SUCCESS)
                return false;
        }
        if (!m_sessionType.isEmpty()) {
            if ((m_result = pam_putenv(m_handle, qPrintable("XDG_SESSION_TYPE=" + m_sessionType))) != PAM_SUCCESS)
                return false;
        }
        if (!m_sessionDesktop.isEmpty()) {
            if ((m_result = pam_putenv(m_handle, qPrintable("XDG_SESSION_DESKTOP=" + m_sessionDesktop))) != PAM_SUCCESS)
                return false;
        }

        // set seat name saving logind PAM module from trying to find it, because it's doesn't
        // find it all the time if we just started the X server
        if (m_display && m_display->seat()) {
            if ((m_result = pam_putenv(m_handle, qPrintable("XDG_SEAT=" + m_display->seat()->name()))) != PAM_SUCCESS)
                return false;
        }
        if (m_display) {
            if ((m_result = pam_putenv(m_handle, qPrintable("XDG_VTNR=" + QString::number(m_display->terminalId())))) != PAM_SUCCESS)
                return false;
        }

        // open session
        if ((m_result = pam_open_session(m_handle, 0)) != PAM_SUCCESS)
            return false;

        // get mapped user name; PAM may have changed it
        if ((m_result = pam_get_item(m_handle, PAM_USER, (const void **)mapped)) != PAM_SUCCESS)
            return false;

        return true;
    }
}
