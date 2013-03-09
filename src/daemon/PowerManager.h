/***************************************************************************
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

#ifndef SDE_POWERMANAGER_H
#define SDE_POWERMANAGER_H

#include <QObject>

#include "Messages.h"

namespace SDE {
    class PowerManagerPrivate;

    class PowerManager : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(PowerManager)
    public:
        PowerManager(QObject *parent = 0);
        ~PowerManager();

    public slots:
        Capabilities capabilities() const;

        void powerOff();
        void reboot();
        void suspend();
        void hibernate();
        void hybridSleep();

    private:
        PowerManagerPrivate *d { nullptr };
    };
}

#endif // SDE_POWERMANAGER_H
