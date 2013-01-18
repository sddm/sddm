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

#include "Cookie.h"

#include "Configuration.h"

#include <unistd.h>

namespace SDE {
    Cookie::Cookie() {
        // create random seed
        struct timespec ts;
        long pid = getpid();
        long tm = time(NULL);

        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
            ts.tv_sec = ts.tv_nsec = 0;
        // initialize random seed
        srandom(pid + tm + (ts.tv_sec ^ ts.tv_nsec));
        // initialize cookie
        strcpy(cookie, "0123456789abcdef0123456789abcdef");
        // create a random hexadecimal number
        const char *digits = "0123456789abcdef";
        for (size_t i = 0; i < 32; ++i)
            cookie[i] = digits[random() & 0x000f];
    }

    bool Cookie::add(const QString &displayName, const QString &authPath) const {
        QString cmd = QString("%1 -f %2 -q").arg(Configuration::instance()->xauthPath()).arg(authPath);
        // execute xauth
        FILE *fp = popen(cmd.toStdString().c_str(), "w");
        // check file
        if (!fp)
            return false;
        fprintf(fp, "remove %s\n", displayName.toStdString().c_str());
        fprintf(fp, "add %s %s %s\n", displayName.toStdString().c_str(), ".", cookie);
        fprintf(fp, "exit\n");
        // close pipe
        pclose(fp);
        // return success
        return true;
    }
}
