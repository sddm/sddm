/*
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef AUTHENUMS_H
#define AUTHENUMS_H

#include <QObject>

namespace SDDM {

class AuthEnums : public QObject {

    Q_OBJECT

public:
    enum Info {
        INFO_NONE = 0,
        INFO_UNKNOWN,
        INFO_PAM_CONV,
        INFO_PASS_CHANGE_REQUIRED,
        _INFO_LAST
    };
    Q_ENUM(Info)

    enum Error {
        ERROR_NONE = 0,
        ERROR_UNKNOWN,
        ERROR_PAM_CONV,
        ERROR_AUTHENTICATION,
        ERROR_INTERNAL,
        _ERROR_LAST
    };
    Q_ENUM(Error)

    enum HelperExitStatus {
        HELPER_SUCCESS = 0,
        HELPER_AUTH_ERROR,
        HELPER_SESSION_ERROR,
        HELPER_OTHER_ERROR,
        HELPER_DISPLAYSERVER_ERROR,
        HELPER_TTY_ERROR,
    };
    Q_ENUM(HelperExitStatus)
};

}

#endif // AUTHENUMS_H
