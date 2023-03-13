/*
 * /etc/passwd authentication backend
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#include "PasswdBackend.h"

#include "AuthEnums.h"
#include "AuthBase.h"
#include "HelperApp.h"

#include <QtCore/QDebug>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#ifdef HAVE_GETSPNAM
#include <shadow.h>
#endif

namespace SDDM {
    PasswdBackend::PasswdBackend(HelperApp *parent)
            : Backend(parent) { }

    bool PasswdBackend::authenticate() {
        if (m_autologin)
            return true;

        if (m_user == QLatin1String("sddm")) {
            if (m_greeter)
                return true;
            else
                return false;
        }

        Request r;
        QString password;

        if (m_user.isEmpty())
            r.prompts << Prompt(AuthPrompt::LOGIN_USER, QStringLiteral("Login"), false);
        r.prompts << Prompt(AuthPrompt::LOGIN_PASSWORD, QStringLiteral("Password"), true);

        bool canceled = false;
        Request response = m_app->request(r,canceled);
        if(canceled==true) return false; // canceled in greeter
        for(const Prompt &p : qAsConst(response.prompts)) {
            switch (p.type) {
                case AuthPrompt::LOGIN_USER:
                    m_user = QString::fromUtf8(p.response);
                    break;
                case AuthPrompt::LOGIN_PASSWORD:
                    password = QString::fromUtf8(p.response);
                    break;
                default:
                    break;
            }
        }

        struct passwd *pw = getpwnam(qPrintable(m_user));
        if (!pw) {
            m_app->error(QStringLiteral("Wrong user/password combination"), AuthEnums::ERROR_AUTHENTICATION, 0);
            return false;
        }
        const char *system_passwd = pw->pw_passwd;

#ifdef HAVE_GETSPNAM
        struct spwd *spw = getspnam(pw->pw_name);
        if (!spw) {
            qWarning() << "[Passwd] Could get passwd but not shadow";
            return false;
        }

        if(!spw->sp_pwdp || !spw->sp_pwdp[0])
            return true;

        system_passwd = spw->sp_pwdp;
#endif

        const char * const crypted = crypt(qPrintable(password), system_passwd);
        if (0 == strcmp(crypted, system_passwd)) {
            return true;
        }

        m_app->error(QStringLiteral("Wrong user/password combination"), AuthEnums::ERROR_AUTHENTICATION, 0);
        return false;
    }

    bool PasswdBackend::start(const QString& user) {
        m_user = user;
        return true;
    }

    QString PasswdBackend::userName() {
        return m_user;
    }
}
