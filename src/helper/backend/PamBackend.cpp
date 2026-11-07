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
#include "VirtualTerminal.h"

#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>

#include <stdlib.h>

namespace SDDM {
    PamBackend::PamBackend(HelperApp *parent)
            : Backend(parent)
            , m_pam(new PamHandle(this)) {
    }

    PamBackend::~PamBackend() {
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
        const auto sessionType = sessionEnv.value(QStringLiteral("XDG_SESSION_TYPE"));
        const auto sessionClass = sessionEnv.value(QStringLiteral("XDG_SESSION_CLASS"));
        if (sessionEnv.contains(QStringLiteral("XDG_VTNR"))) {
            QString tty = VirtualTerminal::path(sessionEnv.value(QStringLiteral("XDG_VTNR")).toInt());
            m_pam->setItem(PAM_TTY, qPrintable(tty));
        }
        if (sessionType == QLatin1String("x11") && (sessionClass == QLatin1String("user") || !m_displayServer)) {
            QString display = sessionEnv.value(QStringLiteral("DISPLAY"));
            if (!display.isEmpty()) {
#ifdef PAM_XDISPLAY
                m_pam->setItem(PAM_XDISPLAY, qPrintable(display));
#else
                m_pam->setItem(PAM_TTY, qPrintable(display));
#endif
            }
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

    QString PamBackend::userName() {
        return QString::fromLocal8Bit((const char*) m_pam->getItem(PAM_USER));
    }

    int PamBackend::converse(int n, const struct pam_message **msg, struct pam_response **resp) {
        qDebug() << "[PAM] Conversation with" << n << "messages";

        if (n <= 0 || n > PAM_MAX_NUM_MSG || !resp || !msg) return PAM_CONV_ERR;
        QVector<size_t> lengths(n, 0);
        *resp = static_cast<pam_response *>(calloc(n, sizeof(pam_response)));
        if (!*resp) return PAM_BUF_ERR;
        for (int i = 0; i < n; ++i) {
            if (!msg[i] || !msg[i]->msg || strlen(msg[i]->msg) > 8192) goto failed;
            if (msg[i]->msg_style == PAM_TEXT_INFO) {
                m_app->info(QString::fromLocal8Bit(msg[i]->msg), Auth::INFO_NONE);
            } else if (msg[i]->msg_style == PAM_ERROR_MSG) {
                m_app->error(QString::fromLocal8Bit(msg[i]->msg), Auth::ERROR_AUTHENTICATION);
            } else if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF || msg[i]->msg_style == PAM_PROMPT_ECHO_ON) {
                Request sent({Prompt(AuthPrompt::UNKNOWN, QString::fromLocal8Bit(msg[i]->msg), msg[i]->msg_style == PAM_PROMPT_ECHO_OFF)});
                Request received = m_app->request(sent);
                if (!received.valid() || received.prompts.length() != 1
                    || received.prompts[0].message != sent.prompts[0].message
                    || received.prompts[0].hidden != sent.prompts[0].hidden
                    || received.prompts[0].response.length() > 16384) goto failed;
                const QByteArray &response = received.prompts[0].response;
                lengths[i] = response.length() + 1;
                (*resp)[i].resp = static_cast<char *>(malloc(lengths[i]));
                if (!(*resp)[i].resp) goto failed;
                memcpy((*resp)[i].resp, response.constData(), response.length());
                (*resp)[i].resp[response.length()] = '\0';
                received.clear();
            } else goto failed;
        }
        return PAM_SUCCESS;
failed:
        for (int i = 0; i < n; ++i) if ((*resp)[i].resp) {
            explicit_bzero((*resp)[i].resp, lengths[i]);
            free((*resp)[i].resp);
        }
        free(*resp); *resp = nullptr;
        return PAM_CONV_ERR;
    }
}
