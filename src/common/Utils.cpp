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

#include "Utils.h"

#include <QString>
#include <QStringList>
#include <QFile>
#include <QHash>
#include <QDebug>

#include <security/pam_appl.h>

extern char **environ;

namespace SDDM {

    const QString Utils::generateName(int length) {
        QString digits = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

        // reserve space for name
        QString name;
        name.reserve(length);

        // create random device
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, digits.length() - 1);

        // generate name
        for (int i = 0; i < length; ++i)
            name[i] = digits.at(dis(gen));

        // return result
        return name;
    }

    /** \brief read system locale settings
     *
     * \return false system locale file has no LANG or LC_ALL setting
     */
    bool Utils::readLocaleFile(QProcessEnvironment &env, QString localeFile) {

        bool langEmpty = true;

        QFile file(localeFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug().noquote() << "Locale file" << localeFile;
            QTextStream in(&file);
            while (!in.atEnd()) {
                QStringList parts = in.readLine().split(QLatin1Char('='));
                if (parts.size() >= 2) {
                    env.insert(parts[0], parts[1]);
                    if (parts[0] == QStringLiteral("LANG") ||
                        parts[0] == QStringLiteral("LC_ALL")) {
                        qDebug().noquote().nospace() << "Set locale " << parts[0] << "=" << parts[1];
                        langEmpty = false;
                    }
                }
            }
            file.close();
        } else
            qDebug().noquote() << "Failed to open" << localeFile;

        if (langEmpty)
            env.insert(QStringLiteral("LANG"), QStringLiteral("C"));

        return !langEmpty;
    }

    /** \brief set environment for localization with system locale settings provided in env */
    void Utils::setLocaleEnv(QProcessEnvironment &env) {
        QStringList keys = env.keys();

        for(int i=0; i<keys.size(); i++) {
            const QString &varname = keys.at(i);

            if(varname.startsWith(QStringLiteral("LC_")) || varname == QStringLiteral("LANG")) {
                qputenv(varname.toLocal8Bit().constData(), env.value(varname).toLocal8Bit());
                qDebug().noquote().nospace() << "Set PAM locale: " << varname << "=" << env.value(varname);
            }
        }
    }

    /** \brief Get string representation of pam status code (return codes) for debug logging */
    const QString Utils::pamResultString(int errnum)
    {
        // https://stackoverflow.com/questions/6576036/initialise-global-key-value-hash
        static const QHash<int, QString> statusCodes(
        {
            { PAM_SUCCESS,               QStringLiteral("PAM_SUCCESS")},
            { PAM_BAD_ITEM,              QStringLiteral("PAM_BAD_ITEM")},
            { PAM_PERM_DENIED,           QStringLiteral("PAM_PERM_DENIED")},
            { PAM_SYSTEM_ERR,            QStringLiteral("PAM_SYSTEM_ERR")},
            { PAM_SYMBOL_ERR,            QStringLiteral("PAM_SYMBOL_ERR")},
            { PAM_SERVICE_ERR,           QStringLiteral("PAM_SERVICE_ERR")},
            { PAM_SYSTEM_ERR,            QStringLiteral("PAM_SYSTEM_ERR")},
            { PAM_BUF_ERR,               QStringLiteral("PAM_BUF_ERR")},
            { PAM_CONV_ERR,              QStringLiteral("PAM_CONV_ERR")},
            { PAM_PERM_DENIED,           QStringLiteral("PAM_PERM_DENIED")},
            { PAM_MAXTRIES,              QStringLiteral("PAM_MAXTRIES")},
            { PAM_AUTH_ERR,              QStringLiteral("PAM_AUTH_ERR")},
            { PAM_NEW_AUTHTOK_REQD,      QStringLiteral("PAM_NEW_AUTHTOK_REQD")},
            { PAM_CRED_INSUFFICIENT,     QStringLiteral("PAM_CRED_INSUFFICIENT")},
            { PAM_AUTHINFO_UNAVAIL,      QStringLiteral("PAM_AUTHINFO_UNAVAIL")},
            { PAM_USER_UNKNOWN,          QStringLiteral("PAM_USER_UNKNOWN")},
            { PAM_CRED_UNAVAIL,          QStringLiteral("PAM_CRED_UNAVAIL")},
            { PAM_CRED_EXPIRED,          QStringLiteral("PAM_CRED_EXPIRED")},
            { PAM_CRED_ERR,              QStringLiteral("PAM_CRED_ERR")},
            { PAM_ACCT_EXPIRED,          QStringLiteral("PAM_ACCT_EXPIRED")},
            { PAM_AUTHTOK_EXPIRED,       QStringLiteral("PAM_AUTHTOK_EXPIRED")},
            { PAM_SESSION_ERR,           QStringLiteral("PAM_SESSION_ERR")},
            { PAM_AUTHTOK_ERR,           QStringLiteral("PAM_AUTHTOK_ERR")},
            { PAM_AUTHTOK_RECOVERY_ERR,  QStringLiteral("PAM_AUTHTOK_RECOVERY_ERR")},
            { PAM_AUTHTOK_LOCK_BUSY,     QStringLiteral("PAM_AUTHTOK_LOCK_BUSY")},
            { PAM_AUTHTOK_DISABLE_AGING, QStringLiteral("PAM_AUTHTOK_DISABLE_AGING")},
            { PAM_NO_MODULE_DATA,        QStringLiteral("PAM_NO_MODULE_DATA")},
            { PAM_IGNORE,                QStringLiteral("PAM_IGNORE")},
            { PAM_ABORT,                 QStringLiteral("PAM_ABORT")},
            { PAM_TRY_AGAIN,             QStringLiteral("PAM_TRY_AGAIN")},
            { PAM_MODULE_UNKNOWN,        QStringLiteral("PAM_MODULE_UNKNOWN")},
        });

        static const QString &unknown = QStringLiteral("UNKNOWN");

        if(statusCodes.contains(errnum))
            return statusCodes[errnum];

        // not in our list
        return unknown;
    }

    /** \brief get string representation of pam work state for debug logging */
    const QString Utils::workStateString(PamWorkState work_state)
    {
        static const QHash<int, QString> workState(
        {
            { PamWorkState::STATE_INITIAL,         QStringLiteral("STATE_INITIAL")},
            { PamWorkState::STATE_STARTED,         QStringLiteral("STATE_STARTED")},
            { PamWorkState::STATE_AUTHENTICATE,    QStringLiteral("STATE_AUTHENTICATE")},
            { PamWorkState::STATE_AUTHORIZE,       QStringLiteral("STATE_AUTHORIZE")},
            { PamWorkState::STATE_CHANGEAUTHTOK,   QStringLiteral("STATE_CHANGEAUTHTOK")},
            { PamWorkState::STATE_AUTHENTICATED,   QStringLiteral("STATE_AUTHENTICATED")},
            { PamWorkState::STATE_AUTHORIZED,      QStringLiteral("STATE_AUTHORIZED")},
            { PamWorkState::STATE_CREDITED,        QStringLiteral("STATE_CREDITED")},
            { PamWorkState::STATE_SESSION_STARTED, QStringLiteral("STATE_SESSION_STARTED")},
            { PamWorkState::STATE_FINISHED,        QStringLiteral("STATE_FINISHED")},
        });

        static const QString &unknown = QStringLiteral("UNKNOWN");

        if(workState.contains(work_state))
            return workState[work_state];

        // not in our list
        return unknown;
    }

    /** \internal Get string representation of pam message msg_style for debug logging */
    const QString &Utils::msgStyleString(int msg_style)
    {
        static const QString msgStyle[] = {
            QStringLiteral("PAM_PROMPT_ECHO_OFF"),
            QStringLiteral("PAM_PROMPT_ECHO_ON"),
            QStringLiteral("PAM_ERROR_MSG"),
            QStringLiteral("PAM_TEXT_INFO"),
            QStringLiteral("UNKNOWN"),
        };

        switch(msg_style) {
            case PAM_PROMPT_ECHO_OFF:
                return msgStyle[0]; break;
            case PAM_PROMPT_ECHO_ON:
                return msgStyle[1]; break;
            case PAM_ERROR_MSG:
                return msgStyle[2]; break;
            case PAM_TEXT_INFO:
                return msgStyle[3]; break;
            default: break;
        }

        return msgStyle[4];
    }

    /** \internal Get string representation of AuthEnum Info for debug logging */
    const QString &Utils::authInfoString(AuthEnums::Info info)
    {
        static const QString authEnumInfo[] = {
            QStringLiteral("INFO_PAM_CONV"),
            QStringLiteral("INFO_PASS_CHANGE_REQUIRED"),
            QStringLiteral("INFO_NONE"),
            QStringLiteral("INFO_UNKNOWN"),
            QStringLiteral("UNKNOWN")
        };

        switch(info) {
            case AuthEnums::INFO_PAM_CONV:
                return authEnumInfo[0]; break;
            case AuthEnums::INFO_PASS_CHANGE_REQUIRED:
                return authEnumInfo[1]; break;
            case AuthEnums::INFO_NONE:
                return authEnumInfo[2]; break;
            case AuthEnums::INFO_UNKNOWN:
                return authEnumInfo[3]; break;
            default: break;
        }

        return authEnumInfo[4];
    }

    /** \internal Get string representation of AuthEnum Error for debug logging */
    const QString &Utils::authErrorString(AuthEnums::Error err)
    {
        static const QString authEnumError[] = {
            QStringLiteral("ERROR_PAM_CONV"),
            QStringLiteral("ERROR_AUTHENTICATION"),
            QStringLiteral("ERROR_INTERNAL"),
            QStringLiteral("ERROR_UNKNOWN"),
            QStringLiteral("ERROR_NONE"),
            QStringLiteral("UNKNOWN")
        };

        switch(err) {
            case AuthEnums::ERROR_PAM_CONV:
                return authEnumError[0]; break;
            case AuthEnums::ERROR_AUTHENTICATION:
                return authEnumError[1]; break;
            case AuthEnums::ERROR_INTERNAL:
                return authEnumError[2]; break;
            case AuthEnums::ERROR_UNKNOWN:
                return authEnumError[3]; break;
            case AuthEnums::ERROR_NONE:
                return authEnumError[4]; break;
            default: break;
        }

        return authEnumError[5];
    }
}
