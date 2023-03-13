/*
 * internal PAM stack working states for backend
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

#ifndef PAMWORKSTATE_H
#define PAMWORKSTATE_H

namespace SDDM {

    // tracking state of PAM stack progress
    enum PamWorkState {
        STATE_INITIAL = 0,      ///< pam stack not started, no handle
        STATE_STARTED,          ///< pam_start ok
        STATE_AUTHENTICATE,     ///< pam_authentication running, authentication requested
        STATE_AUTHENTICATED,    ///< pam_authentication or pam_chauthtok ok
        STATE_AUTHORIZE,        ///< pam_acct_mgmt running, authorization requested
        STATE_CHANGEAUTHTOK,    ///< change auth token requested by pam_authentication
        STATE_AUTHORIZED,       ///< pam_acct_mgmt ok
        STATE_CREDITED,         ///< pam_setcred ok
        STATE_SESSION_STARTED,  ///< pam_open_session ok
        STATE_FINISHED,         ///< pam_close_session or pam_end ok
        _STATE_LAST
    };
}

#endif // PAMWORKSTATE_H
