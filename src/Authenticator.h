/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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

#ifndef SDE_AUTHENTICATOR_H
#define SDE_AUTHENTICATOR_H

#include <QString>

namespace SDE {
    class AuthenticatorPrivate;
    class Cookie;

    class Authenticator {
    public:
        Authenticator(const QString &service);
        ~Authenticator();

        void setCookie(const Cookie &cookie);
        void setDisplay(const QString &displayName);
        void setUsername(const QString &username);
        void setPassword(const QString &password);

        bool authenticate();
        bool login(const QString &loginCommand);
    private:
        AuthenticatorPrivate *d { nullptr };
    };
}

#endif // SDE_AUTHENTICATOR_H
