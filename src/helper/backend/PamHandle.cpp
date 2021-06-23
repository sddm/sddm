/*
 * PAM API Qt wrapper
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
#include "PamHandle.h"
#include "PamBackend.h"

#include <QtCore/QDebug>

namespace SDDM {
    bool PamHandle::putEnv(const QProcessEnvironment& env) {
        const auto envs = env.toStringList();
        for (const QString& s : envs) {
            m_result = pam_putenv(m_handle, qPrintable(s));
            if (m_result != PAM_SUCCESS) {
                qWarning() << "[PAM] putEnv:" << pam_strerror(m_handle, m_result);
                return false;
            }
        }
        return true;
    }

    QProcessEnvironment PamHandle::getEnv() {
        QProcessEnvironment env;
        // get pam environment
        char **envlist = pam_getenvlist(m_handle);
        if (envlist == NULL) {
            qWarning() << "[PAM] getEnv: Returned NULL";
            return env;
        }

        // copy it to the env map
        for (int i = 0; envlist[i] != nullptr; ++i) {
            QString s = QString::fromLocal8Bit(envlist[i]);

            // find equal sign
            int index = s.indexOf(QLatin1Char('='));

            // add to the hash
            if (index != -1)
                env.insert(s.left(index), s.mid(index + 1));

            free(envlist[i]);
        }
        free(envlist);
        return env;
    }

    /** \note pam_chauthtok results:
     *
     * \li PAM_AUTHTOK_ERR
     * \li PAM_AUTHTOK_RECOVERY_ERR
     * \li PAM_AUTHTOK_LOCK_BUSY
     * \li PAM_AUTHTOK_DISABLE_AGING
     * \li PAM_PERM_DENIED
     * \li PAM_TRY_AGAIN
     * \li PAM_MAXTRIES
     * \li PAM_USER_UNKNOWN
     * \li PAM_SUCCESS
     */
    bool PamHandle::chAuthTok(int flags) {
        int old_result = m_result;
        do {
            // clear PAM_MAXTRIES result and restore last
            // value from previous round, for pam_conv
            if(m_result == PAM_MAXTRIES)
                m_result = old_result;

            m_result = pam_chauthtok(m_handle, flags | m_silent);

            if (m_result != PAM_SUCCESS) {
                QString errmsg = QString::fromLocal8Bit(pam_strerror(m_handle, m_result));
                qWarning() << "[PAM] chAuthTok:" << errmsg << ", rc =" << m_result;

                if(m_result == PAM_MAXTRIES && m_retryloop) {
                    /* tell greeter about PAM_MAXTRIES result, or user might be
                     * puzzled about multiple question rounds with current password */
                    emit error(errmsg, AuthEnums::Error::ERROR_PAM_CONV, m_result); // instead m_app->error
                }
            }

            if(m_result == PAM_MAXTRIES && m_retryloop)
                qDebug() << "Loop pam_chauthtok and ignore PAM_MAXTRIES...";

        /* ignore PAM_MAXTRIES result and loop chauthtok? */
        } while(m_result == PAM_MAXTRIES && m_retryloop);

        return m_result == PAM_SUCCESS;
    }

    bool PamHandle::acctMgmt(int flags) {
        if(m_workState == STATE_AUTHENTICATED)
            m_workState = STATE_AUTHORIZE;
        m_result = pam_acct_mgmt(m_handle, flags | m_silent);
        if (m_result == PAM_NEW_AUTHTOK_REQD) {
            m_workState = STATE_CHANGEAUTHTOK;
            return chAuthTok(PAM_CHANGE_EXPIRED_AUTHTOK);
        }
        else if (m_result != PAM_SUCCESS) {
            qWarning() << "[PAM] acctMgmt:" << pam_strerror(m_handle, m_result);
            return false;
        }
        return true;
    }

    /** \note pam_authenticate results:
     *
     * \li PAM_ABORT
     * \li PAM_AUTH_ERR
     * \li PAM_CRED_INSUFFICIENT
     * \li PAM_AUTHINFO_UNVAIL
     * \li PAM_MAXTRIES
     * \li PAM_USER_UNKNOWN
     * \li PAM_SUCCESS
     */
    bool PamHandle::authenticate(int flags) {
        qDebug() << "[PAM] Authenticating...";
        if(m_workState == STATE_STARTED)
            m_workState = STATE_AUTHENTICATE;
        m_result = pam_authenticate(m_handle, flags | m_silent);
        if (m_result == PAM_SUCCESS)
            m_workState = STATE_AUTHENTICATED;
        else
            qWarning() << "[PAM] authenticate:" << pam_strerror(m_handle, m_result);
        qDebug() << "[PAM] returning.";
        return m_result == PAM_SUCCESS;
    }

    /** \note pam_setcred results:
     *
     * \li PAM_BUF_ERR
     * \li PAM_CRED_ERR
     * \li PAM_CRED_EXPIRED
     * \li PAM_CRED_UNAVAIL
     * \li PAM_SYSTEM_ERR
     * \li PAM_USER_UNKNOWN
     * \li PAM_SUCCESS
     */
    bool PamHandle::setCred(int flags) {
        m_result = pam_setcred(m_handle, flags | m_silent);
        if (m_result == PAM_SUCCESS) {
            if(m_workState == STATE_AUTHENTICATED)
                m_workState = STATE_CREDITED;
        } else
            qWarning() << "[PAM] setCred:" << pam_strerror(m_handle, m_result);

        return m_result == PAM_SUCCESS;
    }

    bool PamHandle::openSession() {
        m_result = pam_open_session(m_handle, m_silent);
        if (m_result == PAM_SUCCESS) {
            if(m_workState == STATE_CREDITED)
                m_workState = STATE_SESSION_STARTED;
        } else
            qWarning() << "[PAM] openSession:" << pam_strerror(m_handle, m_result);
        m_open = m_result == PAM_SUCCESS;
        return m_open;
    }

    bool PamHandle::closeSession() {
        m_result = pam_close_session(m_handle, m_silent);
        if (m_result != PAM_SUCCESS) {
            qWarning() << "[PAM] closeSession:" << pam_strerror(m_handle, m_result);
        }
        m_workState = STATE_FINISHED;
        return m_result == PAM_SUCCESS;
    }

    bool PamHandle::isOpen() const {
        return m_open;
    }

    bool PamHandle::setItem(int item_type, const void* item) {
        m_result = pam_set_item(m_handle, item_type, item);
        if (m_result != PAM_SUCCESS) {
            qWarning() << "[PAM] setItem:" << pam_strerror(m_handle, m_result);
        }
        return m_result == PAM_SUCCESS;
    }

    const void* PamHandle::getItem(int item_type) {
        const void *item;
        m_result = pam_get_item(m_handle, item_type, &item);
        if (m_result != PAM_SUCCESS) {
            qWarning() << "[PAM] getItem:" << pam_strerror(m_handle, m_result);
        }
        return item;
    }

    /** \note converse results:
     *
     * \li PAM_BUF_ERR
     * \li PAM_CONV_ERR
     * \li PAM_SUCCESS
     */
    int PamHandle::converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
        qDebug() << "[PAM] Preparing to converse...";
        PamBackend *c = static_cast<PamBackend *>(data);
        return c->converse(n, msg, resp);
    }

    /** \note pam_start results:
     *
     * \li PAM_ABORT
     * \li PAM_BUF_ERR
     * \li PAM_SYSTEM_ERR
     * \li PAM_SUCCESS
     */
    bool PamHandle::start(const QString &service, const QString &user) {
        qDebug() << "[PAM] Starting...";
        qDebug() << "[PAM] pam_start( service =" << service << ", user =" << user << ")";
        if (user.isEmpty())
            m_result = pam_start(qPrintable(service), NULL, &m_conv, &m_handle);
        else
            m_result = pam_start(qPrintable(service), qPrintable(user), &m_conv, &m_handle);
        if (m_result == PAM_SUCCESS)
            m_workState = STATE_STARTED;
        else {
            qWarning() << "[PAM] start" << pam_strerror(m_handle, m_result);
            return false;
        }
        return true;
    }

    bool PamHandle::end(int flags) {
        if (!m_handle)
            return false;
        m_result = pam_end(m_handle, m_result | flags);
        // pam finished, ignore rc
        m_workState = STATE_FINISHED;
        if (m_result != PAM_SUCCESS) {
            qWarning() << "[PAM] end:" << pam_strerror(m_handle, m_result);
            return false;
        }
        else {
            qDebug() << "[PAM] Ended.";
        }
        m_handle = NULL;
        return true;
    }

    int PamHandle::getResult() {
        return m_result;
    }

    QString PamHandle::errorString() {
        return QString::fromLocal8Bit(pam_strerror(m_handle, m_result));
    }

    void PamHandle::setRetryLoop(bool loop) {
        m_retryloop = loop;
    }

    PamHandle::PamHandle(PamWorkState &ref, PamBackend *parent)
        : m_workState(ref), // use pam work state from parent
          m_conv{&PamHandle::converse, parent} { // create context
    }

    PamHandle::~PamHandle() {
        // stop service
        end();
    }
}
