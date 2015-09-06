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

#ifndef XCBKEYBOARDBACKEND_H
#define XCBKEYBOARDBACKEND_H

#include <QtCore/QString>

#include "KeyboardBackend.h"

#define explicit explicit_is_keyword_in_cpp
#include <xcb/xkb.h>
#undef explicit

class QSocketNotifier;

namespace SDDM {
    class XcbKeyboardBackend : public KeyboardBackend {
    public:
        XcbKeyboardBackend(KeyboardModelPrivate *kmp);
        virtual ~XcbKeyboardBackend();

        void init() override;
        void disconnect() override;
        void sendChanges() override;
        void dispatchEvents() override;

        void connectEventsDispatcher(KeyboardModel *model) override;

        static QList<QString> parseShortNames(QString text);

    private:
        // Initializers
        void connectToDisplay();
        void initLedMap();
        void initLayouts();
        void initState();

        // Helpers
        QString atomName(xcb_atom_t atom) const;
        QString atomName(xcb_get_atom_name_cookie_t cookie) const;

        uint8_t getIndicatorMask(uint8_t id) const;

        // Connection
        xcb_connection_t *m_conn { nullptr };

        // Socket listener
        QSocketNotifier *m_socket { nullptr };
    };
}

#endif // XCBKEYBOARDBACKEND_H
