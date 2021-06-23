/*
 * PAM results (return values) for themes
 *
 * Solely provides pam result defines via enum for qml context evaluation,
 * e.g. to check for PAM_MAXTRIES error.
 * The pam results are defined in <security/_pam_types.h>.
 *
 * Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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
 *
 */

#ifndef PAMTYPES_H
#define PAMTYPES_H

#include <QObject>

#include <security/pam_appl.h>

namespace SDDM {

class PamTypes : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PamTypes)
    Q_ENUMS(PamResults)

public:
    explicit PamTypes(QObject *parent = 0);

    enum PamResults {
        RESULT_PAM_SUCCESS = PAM_SUCCESS,
        RESULT_PAM_OPEN_ERR = PAM_OPEN_ERR,
        RESULT_PAM_SYMBOL_ERR = PAM_SYMBOL_ERR,
        RESULT_PAM_SERVICE_ERR = PAM_SERVICE_ERR,
        RESULT_PAM_SYSTEM_ERR = PAM_SYSTEM_ERR,
        RESULT_PAM_BUF_ERR = PAM_BUF_ERR,
        RESULT_PAM_PERM_DENIED = PAM_PERM_DENIED,
        RESULT_PAM_AUTH_ERR = PAM_AUTH_ERR,
        RESULT_PAM_CRED_INSUFFICIENT = PAM_CRED_INSUFFICIENT,
        RESULT_PAM_AUTHINFO_UNAVAIL = PAM_AUTHINFO_UNAVAIL,
        RESULT_PAM_USER_UNKNOWN = PAM_USER_UNKNOWN,
        RESULT_PAM_MAXTRIES = PAM_MAXTRIES,
        RESULT_PAM_NEW_AUTHTOK_REQD = PAM_NEW_AUTHTOK_REQD,
        RESULT_PAM_ACCT_EXPIRED = PAM_ACCT_EXPIRED,
        RESULT_PAM_SESSION_ERR = PAM_SESSION_ERR,
        RESULT_PAM_CRED_UNAVAIL = PAM_CRED_UNAVAIL,
        RESULT_PAM_CRED_EXPIRED = PAM_CRED_EXPIRED,
        RESULT_PAM_CRED_ERR = PAM_CRED_ERR,
        RESULT_PAM_NO_MODULE_DATA = PAM_NO_MODULE_DATA,
        RESULT_PAM_CONV_ERR = PAM_CONV_ERR,
        RESULT_PAM_AUTHTOK_ERR = PAM_AUTHTOK_ERR,
        RESULT_PAM_AUTHTOK_RECOVERY_ERR = PAM_AUTHTOK_RECOVERY_ERR,
        RESULT_PAM_AUTHTOK_LOCK_BUSY = PAM_AUTHTOK_LOCK_BUSY,
        RESULT_PAM_AUTHTOK_DISABLE_AGING = PAM_AUTHTOK_DISABLE_AGING,
        RESULT_PAM_TRY_AGAIN = PAM_TRY_AGAIN,
        RESULT_PAM_IGNORE = PAM_IGNORE,
        RESULT_PAM_ABORT = PAM_ABORT,
        RESULT_PAM_AUTHTOK_EXPIRED = PAM_AUTHTOK_EXPIRED,
        RESULT_PAM_MODULE_UNKNOWN = PAM_MODULE_UNKNOWN,
        RESULT_PAM_BAD_ITEM = PAM_BAD_ITEM,
        RESULT_PAM_CONV_AGAIN = PAM_CONV_AGAIN,
        RESULT_PAM_INCOMPLETE = PAM_INCOMPLETE,
        RESULT_PAM_RETURN_VALUES = _PAM_RETURN_VALUES
    };
};

}

#endif // PAMTYPES_H
