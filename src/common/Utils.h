/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
* Copyright (c) 2014 David Edmundson <davidedmundson@kde.org>
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
***************************************************************************/

#ifndef SDDM_UTILS_H
#define SDDM_UTILS_H

#include <random>

#include <QString>
#include <QProcessEnvironment>

#include "../auth/AuthEnums.h"
#include "../helper/backend/PamWorkState.h"

#define PAM_LOG_PREFIX "[PAM] "

// Log pam function status code (rc)
#define LOG_PAM_RESULT(fcn, rc)  \
    qDebug().noquote().nospace() \
        << PAM_LOG_PREFIX        \
        << #fcn << ": rc = "     \
        << Utils::pamResultString(rc)

// Log pam stack work state
#define LOG_WORK_STATE(fcn, val) \
    qDebug().noquote().nospace() \
        << PAM_LOG_PREFIX        \
        << #fcn << ": state = "  \
        << Utils::workStateString(val)

namespace SDDM {

    class Utils
        {
        public:
            Utils() = delete;
            static const QString generateName(int length);
            static bool readLocaleFile(QProcessEnvironment &env, QString localeFile);
            static void setLocaleEnv(QProcessEnvironment &env);

            // to debug pam results in backend, daemon, greeter
            static const QString pamResultString(int errnum);
            static const QString workStateString(PamWorkState work_state);
            static const QString &msgStyleString(int msg_style);
            static const QString &authInfoString(AuthEnums::Info info);
            static const QString &authErrorString(AuthEnums::Error err);
    };
}

#endif // SDDM_UTILS_H
