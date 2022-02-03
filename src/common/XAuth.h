/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#ifndef SDDM_XAUTH_H
#define SDDM_XAUTH_H

#include <QString>

namespace SDDM {

class XAuth
{
public:
    XAuth();

    QString authDirectory() const;
    void setAuthDirectory(const QString &path);

    QString authPath() const;
    QString cookie() const;

    void setup();
    bool addCookie(const QString &display);

    static bool addCookieToFile(const QString &display,
                                const QString &fileName,
                                const QString &cookie);

private:
    bool m_setup = false;
    QString m_authDir;
    QString m_authPath;
    QString m_cookie;
};

} // namespace SDDM

#endif // SDDM_XAUTH_H
