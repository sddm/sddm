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

#ifndef KEYBOARDBACKEND_H
#define KEYBOARDBACKEND_H

namespace SDDM {
    class KeyboardModel;
    class KeyboardModelPrivate;

    class KeyboardBackend {
    public:
        KeyboardBackend(KeyboardModelPrivate *kmp) : d(kmp) {}

        virtual ~KeyboardBackend() {}

        virtual void init() = 0;
        virtual void disconnect() = 0;
        virtual void sendChanges() = 0;
        virtual void dispatchEvents() = 0;

        virtual void connectEventsDispatcher(KeyboardModel *model) = 0;

    protected:
        KeyboardModelPrivate *d;
    };
}

#endif // KEYBOARDBACKEND_H
