/*
 * PAM authentication backend
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
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

#include "PamBackend.h"
#include "PamHandle.h"
#include "HelperApp.h"
#include "UserSession.h"
#include "Auth.h"

#include <QtCore/QString>
#include <QtCore/QDebug>

#include <stdlib.h>

namespace SDDM {
    static Request loginRequest {
        {   { AuthPrompt::LOGIN_USER, "login:", false },
            { AuthPrompt::LOGIN_PASSWORD, "Password: ", true }
        }
    };

    static Request changePassRequest {
        {   { AuthPrompt::CHANGE_CURRENT, "(current) UNIX password: ", true },
            { AuthPrompt::CHANGE_NEW, "New password: ", true },
            { AuthPrompt::CHANGE_REPEAT, "Retype new password: ", true }
        }
    };

    static Request changePassNoOldRequest {
        {   { AuthPrompt::CHANGE_NEW, "New password: ", true },
            { AuthPrompt::CHANGE_REPEAT, "Retype new password: ", true }
        }
    };

    static Request invalidRequest { {} };

    static Prompt invalidPrompt {};

    PamData::PamData() { }

    AuthPrompt::Type PamData::detectPrompt(const struct pam_message* msg) const {
        if (msg->msg_style == PAM_PROMPT_ECHO_OFF) {
            QString message(msg->msg);
            if (message.indexOf(QRegExp("\\bpassword\\b", Qt::CaseInsensitive)) >= 0) {
                if (message.indexOf(QRegExp("\\b(re-?(enter|type)|again|confirm|repeat)\\b", Qt::CaseInsensitive)) >= 0) {
                    return AuthPrompt::CHANGE_REPEAT;
                }
                else if (message.indexOf(QRegExp("\\bnew\\b", Qt::CaseInsensitive)) >= 0) {
                    return AuthPrompt::CHANGE_NEW;
                }
                else if (message.indexOf(QRegExp("\\b(old|current)\\b", Qt::CaseInsensitive)) >= 0) {
                    return AuthPrompt::CHANGE_CURRENT;
                }
                else {
                    return AuthPrompt::LOGIN_PASSWORD;
                }
            }
        }
        else {
            return AuthPrompt::LOGIN_USER;
        }

        return AuthPrompt::UNKNOWN;
    }

    const Prompt& PamData::findPrompt(const struct pam_message* msg) const {
        AuthPrompt::Type type = detectPrompt(msg);

        for (const Prompt &p : m_currentRequest.prompts) {
            if (type == p.type && p.message == msg->msg)
                return p;
        }

        return invalidPrompt;
    }

    Prompt& PamData::findPrompt(const struct pam_message* msg) {
        AuthPrompt::Type type = detectPrompt(msg);

        for (Prompt &p : m_currentRequest.prompts) {
            if (type == AuthPrompt::UNKNOWN && msg->msg == p.message)
                return p;
            if (type == p.type)
                return p;
        }

        return invalidPrompt;
    }

    /*
    * Expects an empty prompt list if the previous request has been processed
    */
    bool PamData::insertPrompt(const struct pam_message* msg, bool predict) {
        Prompt &p = findPrompt(msg);

        // first, check if we already have stored this propmpt
        if (p.valid()) {
            // we have a response already - do nothing
            if (m_sent)
                return false;
            // we don't have a response yet - replace the message and prepare to send it
            p.message = msg->msg;
            return true;
        }
        // this prompt is not stored but we have some prompts
        else if (m_currentRequest.prompts.length() != 0) {
            // check if we have already sent this - if we did, get rid of the answers
            if (m_sent) {
                m_currentRequest.clear();
                m_sent = false;
            }
        }

        // we'll predict what will come next
        if (predict) {
            AuthPrompt::Type type = detectPrompt(msg);
            switch (type) {
                case AuthPrompt::LOGIN_USER:
                    m_currentRequest = Request(loginRequest);
                    return true;
                case AuthPrompt::CHANGE_CURRENT:
                    m_currentRequest = Request(changePassRequest);
                    return true;
                case AuthPrompt::CHANGE_NEW:
                    m_currentRequest = Request(changePassNoOldRequest);
                    return true;
                default:
                    break;
            }
        }

        // or just add whatever comes exactly as it comes
        m_currentRequest.prompts.append(Prompt(detectPrompt(msg), msg->msg, msg->msg_style == PAM_PROMPT_ECHO_OFF));

        return true;
    }

    Auth::Info PamData::handleInfo(const struct pam_message* msg, bool predict) {
        if (QString(msg->msg).indexOf(QRegExp("^Changing password for [^ ]+$"))) {
            if (predict)
                m_currentRequest = Request(changePassRequest);
            return Auth::INFO_PASS_CHANGE_REQUIRED;
        }
        return Auth::INFO_UNKNOWN;
    }

    /*
    * Destroys the prompt with that response
    */
    QByteArray PamData::getResponse(const struct pam_message* msg) {
        QByteArray response = findPrompt(msg).response;
        m_currentRequest.prompts.removeOne(findPrompt(msg));
        if (m_currentRequest.prompts.length() == 0)
            m_sent = false;
        return response;
    }

    const Request& PamData::getRequest() const {
        if (!m_sent)
            return m_currentRequest;
        else
            return invalidRequest;
    }

    void PamData::completeRequest(const Request& request) {
        if (request.prompts.length() != m_currentRequest.prompts.length()) {
            qWarning() << "[PAM] Different request/response list length, ignoring";
            return;
        }

        for (int i = 0; i < request.prompts.length(); i++) {
            if (request.prompts[i].type != m_currentRequest.prompts[i].type
                || request.prompts[i].message != m_currentRequest.prompts[i].message
                || request.prompts[i].hidden != m_currentRequest.prompts[i].hidden) {
                qWarning() << "[PAM] Order or type of the messages doesn't match, ignoring";
                return;
            }
        }

        m_currentRequest = request;
        m_sent = true;
    }




    PamBackend::PamBackend(HelperApp *parent)
            : Backend(parent)
            , m_data(new PamData())
            , m_pam(new PamHandle(this)) {
    }

    PamBackend::~PamBackend() {
        delete m_data;
        delete m_pam;
    }

    bool PamBackend::start(const QString &user) {
        bool result;

        QString service = "sddm";

        if (user == "sddm")
            service = "sddm-greeter";
        else if (m_app->session()->path().isEmpty())
            service = "sddm-check";
        else if (m_autologin)
            service = "sddm-autologin";
        result = m_pam->start(service, user);

        if (!result)
            m_app->error(m_pam->errorString(), Auth::ERROR_INTERNAL);

        return result;
    }

    bool PamBackend::authenticate() {
        if (!m_pam->authenticate()) {
            m_app->error(m_pam->errorString(), Auth::ERROR_AUTHENTICATION);
            return false;
        }
        if (!m_pam->acctMgmt()) {
            m_app->error(m_pam->errorString(), Auth::ERROR_AUTHENTICATION);
            return false;
        }
        return true;
    }

    bool PamBackend::openSession() {
        if (!m_pam->setCred(PAM_ESTABLISH_CRED)) {
            m_app->error(m_pam->errorString(), Auth::ERROR_AUTHENTICATION);
            return false;
        }
        QProcessEnvironment sessionEnv = m_app->session()->processEnvironment();
        QString display = sessionEnv.value("DISPLAY");
        if (!display.isEmpty()) {
            m_pam->setItem(PAM_XDISPLAY, qPrintable(display));
            m_pam->setItem(PAM_TTY, qPrintable(display));
        }
        if (!m_pam->putEnv(sessionEnv)) {
            m_app->error(m_pam->errorString(), Auth::ERROR_INTERNAL);
            return false;
        }
        if (!m_pam->openSession()) {
            m_app->error(m_pam->errorString(), Auth::ERROR_INTERNAL);
            return false;
        }
        sessionEnv.insert(m_pam->getEnv());
        m_app->session()->setProcessEnvironment(sessionEnv);
        return Backend::openSession();
    }

    QString PamBackend::userName() {
        return (const char*) m_pam->getItem(PAM_USER);
    }

    int PamBackend::converse(int n, const struct pam_message **msg, struct pam_response **resp) {
        qDebug() << "[PAM] Conversation with" << n << "messages";

        bool newRequest = false;

        if (n <= 0 || n > PAM_MAX_NUM_MSG)
            return PAM_CONV_ERR;

        for (int i = 0; i < n; i++) {
            switch(msg[i]->msg_style) {
                case PAM_PROMPT_ECHO_OFF:
                case PAM_PROMPT_ECHO_ON:
                    newRequest = m_data->insertPrompt(msg[i], n == 1);
                    break;
                case PAM_ERROR_MSG:
                    m_app->error(msg[i]->msg, Auth::ERROR_AUTHENTICATION);
                    break;
                case PAM_TEXT_INFO:
                    // if there's only the info message, let's predict the prompts too
                    m_app->info(msg[i]->msg, m_data->handleInfo(msg[i], n == 1));
                    break;
                default:
                    break;
            }
        }

        if (newRequest) {
            Request sent = m_data->getRequest();
            Request received;

            if (sent.valid()) {
                received = m_app->request(sent);

                if (!received.valid())
                    return PAM_CONV_ERR;

                m_data->completeRequest(received);
            }
        }

        *resp = (struct pam_response *) calloc(n, sizeof(struct pam_response));
        if (!*resp) {
            return PAM_BUF_ERR;
        }

        for (int i = 0; i < n; i++) {
            QByteArray response = m_data->getResponse(msg[i]);

            resp[i]->resp = (char *) malloc(response.length() + 1);
            // on error, get rid of everything
            if (!resp[i]->resp) {
                for (int j = 0; j < n; j++) {
                    free(resp[i]->resp);
                    resp[i]->resp = nullptr;
                }
                free(*resp);
                *resp = nullptr;
                return PAM_BUF_ERR;
            }

            memcpy(resp[i]->resp, response.constData(), response.length());
            resp[i]->resp[response.length()] = '\0';
        }

        return PAM_SUCCESS;
    }
}
