/*
 * PAM authentication backend
 * Copyright (c) 2013 Martin Bříza <mbriza@redhat.com>
 * Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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
#include "Utils.h"

#include <QtCore/QString>
#include <QtCore/QDebug>

#include <stdlib.h>

namespace SDDM {

    static Request invalidRequest { {} };

    static Prompt invalidPrompt {};

    PamData::PamData(PamWorkState &ref) : m_workState(ref) { }

    AuthPrompt::Type PamData::detectPrompt(const struct pam_message* msg) const {
        if(m_workState == STATE_AUTHENTICATE) {
            if (msg->msg_style == PAM_PROMPT_ECHO_OFF)
                return AuthPrompt::LOGIN_PASSWORD;
            else if (msg->msg_style == PAM_PROMPT_ECHO_ON)
                return AuthPrompt::LOGIN_USER;
        }
        else if(m_workState == STATE_CHANGEAUTHTOK) {
            if (msg->msg_style == PAM_PROMPT_ECHO_OFF)
                return AuthPrompt::CHANGE_PASSWORD;
            else if (msg->msg_style == PAM_PROMPT_ECHO_ON)
                // unlikely but handle
                return AuthPrompt::LOGIN_USER;
        }

        return AuthPrompt::UNKNOWN;
    }

    const Prompt& PamData::findPrompt(const struct pam_message* msg) const {
        AuthPrompt::Type type = detectPrompt(msg);

        for (const Prompt &p : m_currentRequest.prompts) {
            if (type == p.type && p.message == QString::fromLocal8Bit(msg->msg))
                return p;
        }

        return invalidPrompt;
    }

    Prompt& PamData::findPrompt(const struct pam_message* msg) {
        AuthPrompt::Type type = detectPrompt(msg);

        for (Prompt &p : m_currentRequest.prompts) {
            if (type == AuthPrompt::UNKNOWN && QString::fromLocal8Bit(msg->msg) == p.message)
                return p;
            if (type == p.type)
                return p;
        }

        return invalidPrompt;
    }

    /*
    * Expects an empty prompt list if the previous request has been processed
    *
    * @return true if new prompt was inserted or prompt message was set
    * and false if prompt was found and already sent
    */
    bool PamData::insertPrompt(const struct pam_message* msg, bool predict) {
        Prompt &p = findPrompt(msg);

        // first, check if we already have stored this prompt
        if (p.valid()) {
            // we have a response already - do nothing
            if (m_sent)
                return false;
            // we don't have a response yet - replace the message and prepare to send it
            p.message = QString::fromLocal8Bit(msg->msg);
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
            m_sent = false;

            switch (type) {
                case AuthPrompt::LOGIN_USER:
                case AuthPrompt::LOGIN_PASSWORD:
                case AuthPrompt::CHANGE_PASSWORD:
                    m_currentRequest = Request( { { type, QString::fromLocal8Bit(msg->msg),
                                                    type == AuthPrompt::LOGIN_USER ? true : false } } );
                    return true;
                default:
                    break;
            }
        }

        // or just add whatever comes exactly as it comes
        m_currentRequest.prompts.append(Prompt(detectPrompt(msg),
                                        QString::fromLocal8Bit(msg->msg),
                                        msg->msg_style == PAM_PROMPT_ECHO_OFF));

        return true;
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

    /* As long as m_currentRequest is not sent (m_sent is false) return current request */
    const Request& PamData::getRequest() const {
        if (!m_sent)
            return m_currentRequest;
        else
            return invalidRequest;
    }

    /* Use new request if prompts are equal to current one and set sent true. */
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
            , m_data(new PamData(m_workState))
            , m_pam(new PamHandle(m_workState, this)) {

        // inform daemon in case pam_chauthtok fails with PAM_MAXTRIES
        QObject::connect(m_pam, SIGNAL(error(QString, AuthEnums::Error, int)),
                         parent, SLOT(error(QString, AuthEnums::Error, int)));
    }

    PamBackend::~PamBackend() {
        delete m_data;
        delete m_pam;
    }

    bool PamBackend::start(const QString &user) {
        bool result;

        QString service = QStringLiteral("sddm");

        if (user == QStringLiteral("sddm") && m_greeter)
            service = QStringLiteral("sddm-greeter");
        else if (m_autologin)
            service = QStringLiteral("sddm-autologin");
        result = m_pam->start(service, user);

        if (!result)
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_INTERNAL, m_pam->getResult());

        return result;
    }

    bool PamBackend::authenticate() {
        m_convCanceled = false; // reset for converse()
        if (!m_pam->authenticate()) {
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_AUTHENTICATION, m_pam->getResult());
            return false;
        }
        if (!m_pam->acctMgmt()) {
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_AUTHENTICATION, m_pam->getResult());
            return false;
        }
        return true;
    }

    bool PamBackend::openSession() {
        if (!m_pam->setCred(PAM_ESTABLISH_CRED)) {
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_AUTHENTICATION, m_pam->getResult());
            return false;
        }

        QProcessEnvironment sessionEnv = m_app->session()->processEnvironment();
        const auto sessionType = sessionEnv.value(QStringLiteral("XDG_SESSION_TYPE"));
        const auto sessionClass = sessionEnv.value(QStringLiteral("XDG_SESSION_CLASS"));
        if (sessionType == QLatin1String("x11") && (sessionClass == QLatin1String("user") || !m_displayServer)) {
            QString display = sessionEnv.value(QStringLiteral("DISPLAY"));
            if (!display.isEmpty()) {
#ifdef PAM_XDISPLAY
                m_pam->setItem(PAM_XDISPLAY, qPrintable(display));
#endif
                m_pam->setItem(PAM_TTY, qPrintable(display));
            }
        } else {
            QString tty = QStringLiteral("/dev/tty%1").arg(sessionEnv.value(QStringLiteral("XDG_VTNR")));
            m_pam->setItem(PAM_TTY, qPrintable(tty));
        }

        if (!m_pam->putEnv(sessionEnv)) {
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_INTERNAL, m_pam->getResult());
            return false;
        }
        if (!m_pam->openSession()) {
            m_app->error(m_pam->errorString(), AuthEnums::ERROR_INTERNAL, m_pam->getResult());
            return false;
        }
        sessionEnv.insert(m_pam->getEnv());
        m_app->session()->setProcessEnvironment(sessionEnv);
        return Backend::openSession();
    }

    bool PamBackend::closeSession() {
        if (m_pam->isOpen()) {
            qDebug() << "[PAM] Closing session";
            m_pam->closeSession();
            m_pam->setCred(PAM_DELETE_CRED);
            return true;
        }
        qWarning() << "[PAM] Asked to close the session but it wasn't previously open";
        return Backend::closeSession();
    }

    /** what to do when pam_chauthtok (password expired) reaches retry limit
     * and fails with PAM_MAXTRIES: continue in loop or give up and return? */
    void PamBackend::setRetryLoop(bool loop) {
        m_pam->setRetryLoop(loop);
    }

    QString PamBackend::userName() {
        return QString::fromLocal8Bit((const char*) m_pam->getItem(PAM_USER));
    }

    int PamBackend::converse(int n, const struct pam_message **msg, struct pam_response **resp) {
        qDebug() << "[PAM] Conversation with" << n << "messages";

        bool newRequest = false;

        if (n <= 0 || n > PAM_MAX_NUM_MSG)
            return PAM_CONV_ERR;

        LOG_WORK_STATE(pam_conv, m_workState);

        // see whats going on in pam_conv
        for (int i = 0; i < n; i++) {
            qDebug().noquote().nospace() << "[PAM] pam_conv: style = "
                                         << Utils::msgStyleString(msg[i]->msg_style)
                                         << " (" << msg[i]->msg_style << "), msg[" << i << "] = \""
                                         << QString::fromLocal8Bit(msg[i]->msg) << "\"";
        }

        for (int i = 0; i < n; i++) {

            QString convMsg = QString::fromLocal8Bit(msg[i]->msg);

            switch(msg[i]->msg_style) {
                // request password
                case PAM_PROMPT_ECHO_OFF:
                // request user name
                case PAM_PROMPT_ECHO_ON:
                    newRequest = m_data->insertPrompt(msg[i], n == 1);
                    break;
                case PAM_ERROR_MSG:
                    qDebug() << "[PAM] PamBackend: pam error message, msg=" << convMsg;
                    m_app->error(convMsg, AuthEnums::ERROR_PAM_CONV, m_pam->getResult());
                    break;
                case PAM_TEXT_INFO:
                    // send pam conversation msg to greeter via HelperApp, SocketServer, Display
                    qDebug() << "[PAM] PamBackend: pam info message, msg=" << convMsg;
                    m_app->info(convMsg, AuthEnums::INFO_PAM_CONV, m_pam->getResult());
                    break;
                default:
                    break;
            }
        }

        if (newRequest) {
            // get current request
            Request send = m_data->getRequest();
            Request received;

            // any prompt?
            if (send.valid()) {
                // send request to daemon (ask for a response)
                received = m_app->request(send, m_convCanceled);

                // password input canceled in greeter
                if (m_convCanceled) {
                    return PAM_CONV_ERR;
                }

                if (!received.valid())
                    return PAM_CONV_ERR;

                // compare request with response
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
                for (int j = 0; j < i; j++) {
                    free(resp[j]->resp);
                    resp[j]->resp = nullptr;
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
