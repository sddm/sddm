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

#ifndef SDE_UTIL_H
#define SDE_UTIL_H

#include <QString>
#include <QStringList>

#include <functional>

#include <unistd.h>

namespace SDE {
    class Util {
    public:
        Util() = delete;

        static pid_t execute(std::function<void (void)> function);
        static pid_t execute(const QString &program, const QStringList &arguments, std::function<void (void)> function);
        static int wait(pid_t pid);
        static int terminate(pid_t pid);
    };
}

#endif // SDE_UTIL_H
