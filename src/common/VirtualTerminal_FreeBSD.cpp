/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QDebug>
#include <QString>

#include "VirtualTerminal.h"

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/consio.h>


namespace SDDM {
    namespace VirtualTerminal {
        int setUpNewVt() {
            int fd = ::open("/dev/console", O_RDONLY);
            if(fd == -1) {
                qDebug() << "Failed to open /dev/console: " << strerror(errno);
                return -1;
            }

            int vt;
            int err = ::ioctl(fd, VT_OPENQRY, &vt);
            if(err == -1) {
                qDebug() << "ioctl(VT_OPENQRY) failed: " << strerror(errno);
                return -1;
            }

            return vt;
        }

        void jumpToVt(int vt, bool vt_auto) {
            qDebug() << "Jumping to VT" << vt << "is unsupported on FreeBSD";
        }
    }
}
