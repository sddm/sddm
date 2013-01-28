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

#ifndef SDE_DISPLAYMANAGER_H
#define SDE_DISPLAYMANAGER_H

#include <QString>

namespace SDE {
    class DisplayManagerPrivate;

    class DisplayManager {
    public:
        DisplayManager();
        ~DisplayManager();

        void setCookie(const char *cookie);
        void setDisplay(const QString &display);

        bool start();
        bool stop();

    private:
        DisplayManagerPrivate *d { nullptr };
    };
}
#endif // SDE_DISPLAYMANAGER_H
