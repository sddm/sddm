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

#ifndef SDDM_SEAT_H
#define SDDM_SEAT_H

#include <QObject>
#include <QVector>
#include "Display.h"

namespace SDDM {
    class Display;

    class Seat : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Seat)
    public:
        explicit Seat(const QString &name, QObject *parent = 0);

        const QString &name() const;
        void createDisplay(Display::DisplayServerType serverType);

    public slots:
        void removeDisplay(SDDM::Display* display);

    private slots:
        void displayStopped();

    private:
        void startDisplay(SDDM::Display *display, int tryNr = 1);

        QString m_name;

        QVector<Display *> m_displays;
    };
}

#endif // SDDM_SEAT_H
