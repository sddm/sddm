/***************************************************************************
* Copyright (c) 2013 Nikita Mikhaylov <nslqqq@gmail.com>
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

#ifndef KEYBOARDLAYOUT_H
#define KEYBOARDLAYOUT_H

#include <QtCore/QObject>

namespace SDDM {
    class KeyboardLayout : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString shortName READ shortName CONSTANT)
        Q_PROPERTY(QString longName READ longName CONSTANT)
    public:
        KeyboardLayout(QString shortName, QString longName);

        virtual ~KeyboardLayout() = default;

        QString shortName() const;
        QString longName() const;

    private:
        QString m_short, m_long;
    };
}

#endif // KEYBOARDLAYOUT_H
