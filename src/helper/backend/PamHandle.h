/*
 * PAM API Qt wrapper
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

#ifndef PAMHANDLE_H
#define PAMHANDLE_H

#include <QtCore/QObject>
#include <QtCore/QProcessEnvironment>
#include <security/pam_appl.h>

namespace SDDM {
    class PamBackend;
    /**
    * Class wrapping the standard Linux-PAM library calls
    *
    * Almost everything is left the same except the following things:
    *
    * Mainly, state returns - if you call pam_start, pam_open_session and then
    * pam_start again, the session will get closed and the conversation closed
    *
    * You don't need to pass PAM_SILENT to every call if you want PAM to be quiet.
    * You can set the flag globally by using the \ref setSilence method.
    *
    * \ref acctMgmt doesn't require you to handle the PAM_NEW_AUTHTOK_REQD condition,
    * it calls chAuthTok on its own.
    *
    * Error messages are automatically reported to qDebug
    */
    class PamHandle {
    public:
        /**
        * ctor
        * \param parent parent backend
        */
        explicit PamHandle(PamBackend *parent = 0);

        virtual ~PamHandle();

        /**
        * pam_set_item - set and update PAM informations
        *
        * \param item_type PAM item type
        * \param item item pointer
        *
        * \return true on success
        */
        bool setItem(int item_type, const void *item);

        /**
        * pam_get_item - getting PAM informations
        *
        * \param item_type
        *
        * \return item pointer or NULL on failure
        */
        const void *getItem(int item_type);

        /**
        * pam_open_session - start PAM session management
        *
        * \return true on success
        */
        bool openSession();

        /**
        * pam_close_session - terminate PAM session management
        *
        * \return true on success
        */
        bool closeSession();

        /**
        * pam_setcred - establish / delete user credentials
        *
        * \param flags PAM flag(s)
        *
        * \return true on success
        */
        bool setCred(int flags = 0);

        /**
        * pam_authenticate - account authentication
        *
        * \param flags PAM flag(s)
        *
        * \return true on success
        */
        bool authenticate(int flags = 0);

        /**
        * pam_acct_mgmt - PAM account validation management
        *
        * @note Automatically calls \ref chAuthTok if the password is expired
        *
        * \param flags PAM flag(s)
        *
        * \return true on success
        */
        bool acctMgmt(int flags = 0);

        /**
        * pam_chauthtok - updating authentication tokens
        *
        * \param flags PAM flag(s)
        *
        * \return true on success
        */
        bool chAuthTok(int flags = 0);

        /**
        * pam_getenv - get PAM environment
        *
        * \return Complete process environment
        */
        QProcessEnvironment getEnv();

        /**
        * pam_putenv - set or change PAM environment
        *
        * \param env environment to be merged into the PAM one
        *
        * \return true on success
        */
        bool putEnv(const QProcessEnvironment& env);

        /**
        * pam_end - termination of PAM transaction
        *
        * \param flags to be OR'd with the status (PAM_DATA_SILENT)
        * \return true on success
        */
        bool end(int flags = 0);

        /**
        * pam_start - initialization of PAM transaction
        *
        * \param service PAM service name, e.g. "sddm"
        * \param pam_conversation pointer to the PAM conversation structure to be used
        * \param user username
        *
        * \return true on success
        */
        bool start(const QString &service, const QString &user = QString());

        /**
        * Set PAM_SILENT upon the contained calls
        * \param silent true if silent
        */
        void setSilence(bool silent);

        /**
        * Generates an error message according to the internal state
        *
        * \return error string
        */
        QString errorString();

    private:
        /**
        * Conversation function for the pam_conv structure
        *
        * Calls ((PamHandle*)pam_conv.appdata_ptr)->doConverse() with its parameters
        *
        * Not to be called directly, therefore private
        */
        static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data);

        int m_silent { 0 }; ///< flag mask for silence of the contained calls

        struct pam_conv m_conv; ///< the current conversation
        pam_handle_t *m_handle { nullptr }; ///< the actual PAM handle
        int m_result { 0 }; ///< PAM result
    };
}

#endif // PAMHANDLE_H
