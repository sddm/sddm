/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QDebug>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    bool changeOwner(const QString &fileName) {
        // change the owner and group of the file to the sddm user
        struct passwd *pw = getpwnam("sddm");
        if (pw) {
            if (chown(qPrintable(fileName), pw->pw_uid, pw->pw_gid) == -1) {
                qCritical() << "Failed to change owner of" << fileName;
                return false;
            }
        } else {
            qCritical("Cannot change owner of \"%s\": failed to find the sddm user",
                      qPrintable(fileName));
            return false;
        }

        return true;
    }
}
