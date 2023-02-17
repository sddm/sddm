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

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QRegularExpression>

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#include "KeyboardLayout.h"
#include "XcbKeyboardBackend.h"

#include <QSocketNotifier>

namespace SDDM {
    XcbKeyboardBackend::XcbKeyboardBackend(KeyboardModelPrivate *kmp) : KeyboardBackend(kmp) {
    }

    XcbKeyboardBackend::~XcbKeyboardBackend() {
    }

    void XcbKeyboardBackend::init() {
        connectToDisplay();
        if (d->enabled)
            initLedMap();
        if (d->enabled)
            initLayouts();
        if (d->enabled)
            initState();
    }

    void XcbKeyboardBackend::disconnect() {
        delete m_socket;
        xcb_disconnect(m_conn);
    }

    void XcbKeyboardBackend::sendChanges() {
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *error = nullptr;

        // Compute masks
        uint8_t mask_full = d->numlock.mask | d->capslock.mask,
                mask_cur  = (d->numlock.enabled  ? d->numlock.mask  : 0) |
                            (d->capslock.enabled ? d->capslock.mask : 0);

        // Change state
        cookie = xcb_xkb_latch_lock_state(m_conn,
                    XCB_XKB_ID_USE_CORE_KBD,
                    mask_full,
                    mask_cur,
                    1,
                    d->layout_id,
                    0, 0, 0);
        error = xcb_request_check(m_conn, cookie);

        if (error) {
            qWarning() << "Can't update state: " << error->error_code;
        }
    }

    void XcbKeyboardBackend::connectToDisplay() {
        // Connect and initialize xkb extension
        xcb_xkb_use_extension_cookie_t cookie;
        xcb_generic_error_t *error = nullptr;

        m_conn = xcb_connect(nullptr, nullptr);
        if (m_conn == nullptr) {
            qCritical() << "xcb_connect failed, keyboard extension disabled";
            d->enabled = false;
            return;
        }

        // Initialize xkb extension
        cookie = xcb_xkb_use_extension(m_conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        xcb_xkb_use_extension_reply(m_conn, cookie, &error);

        if (error != nullptr) {
            qCritical() << "xcb_xkb_use_extension failed, extension disabled, error code"
                        << error->error_code;
            d->enabled = false;
            return;
        }
    }

    void XcbKeyboardBackend::initLedMap() {
        // Get indicator names atoms
        xcb_xkb_get_names_cookie_t cookie;
        xcb_xkb_get_names_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        cookie = xcb_xkb_get_names(m_conn,
                XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_NAME_DETAIL_INDICATOR_NAMES);
        reply = xcb_xkb_get_names_reply(m_conn, cookie, &error);

        if (error) {
            qCritical() << "Can't init led map: " << error->error_code;
            d->enabled = false;
            return;
        }

        // Unpack
        xcb_xkb_get_names_value_list_t list;
        const void *buffer = xcb_xkb_get_names_value_list(reply);
        xcb_xkb_get_names_value_list_unpack(buffer, reply->nTypes, reply->indicators,
                reply->virtualMods, reply->groupNames, reply->nKeys, reply->nKeyAliases,
                reply->nRadioGroups, reply->which, &list);

        // Get indicators count
        int ind_cnt = xcb_xkb_get_names_value_list_indicator_names_length(reply, &list);

        // Loop through indicators and get their properties
        QList<xcb_get_atom_name_cookie_t> cookies;
        for (int i = 0; i < ind_cnt; i++) {
            cookies << xcb_get_atom_name(m_conn, list.indicatorNames[i]);
        }

        for (int i = 0; i < ind_cnt; i++) {
            QString name = atomName(cookies[i]);

            if (name == QLatin1String("Num Lock")) {
                d->numlock.mask = getIndicatorMask(i);
            } else if (name == QLatin1String("Caps Lock")) {
                d->capslock.mask = getIndicatorMask(i);
            }
        }

        // Free memory
        free(reply);
    }

    void XcbKeyboardBackend::initLayouts() {
        xcb_xkb_get_names_cookie_t cookie;
        xcb_xkb_get_names_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get atoms for short and long names
        cookie = xcb_xkb_get_names(m_conn,
                XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_NAME_DETAIL_GROUP_NAMES | XCB_XKB_NAME_DETAIL_SYMBOLS);
        reply = xcb_xkb_get_names_reply(m_conn, cookie, nullptr);

        if (error) {
            // Log and disable
            qCritical() << "Can't init layouts: " << error->error_code;
            return;
        }

        // Unpack
        const void *buffer = xcb_xkb_get_names_value_list(reply);
        xcb_xkb_get_names_value_list_t res_list;
        xcb_xkb_get_names_value_list_unpack(buffer, reply->nTypes, reply->indicators,
                reply->virtualMods, reply->groupNames, reply->nKeys, reply->nKeyAliases,
                reply->nRadioGroups, reply->which, &res_list);

        // Get short names
        QList<QString> short_names = parseShortNames(atomName(res_list.symbolsName));

        // Loop through group names
        d->layouts.clear();
        int groups_cnt = xcb_xkb_get_names_value_list_groups_length(reply, &res_list);

        QList<xcb_get_atom_name_cookie_t> cookies;
        for (int i = 0; i < groups_cnt; i++) {
            cookies << xcb_get_atom_name(m_conn, res_list.groups[i]);
        }

        for (int i = 0; i < groups_cnt; i++) {
            QString nshort, nlong = atomName(cookies[i]);
            if (i < short_names.length())
                nshort = short_names[i];

            d->layouts << new KeyboardLayout(nshort, nlong);
        }

        // Free
        free(reply);
    }

    void XcbKeyboardBackend::initState() {
        xcb_xkb_get_state_cookie_t cookie;
        xcb_xkb_get_state_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get xkb state
        cookie = xcb_xkb_get_state(m_conn, XCB_XKB_ID_USE_CORE_KBD);
        reply = xcb_xkb_get_state_reply(m_conn, cookie, &error);

        if (reply) {
            // Set locks state
            d->capslock.enabled = reply->lockedMods & d->capslock.mask;
            d->numlock.enabled  = reply->lockedMods & d->numlock.mask;

            // Set current layout
            d->layout_id = reply->group;

            // Free
            free(reply);
        } else {
            // Log error and disable extension
            qCritical() << "Can't load leds state - " << error->error_code;
            d->enabled = false;
        }
    }

    QString XcbKeyboardBackend::atomName(xcb_get_atom_name_cookie_t cookie) const {
        xcb_get_atom_name_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get atom name
        reply = xcb_get_atom_name_reply(m_conn, cookie, &error);

        QString res;

        if (reply) {
            QByteArray replyText(xcb_get_atom_name_name(reply),
                                 xcb_get_atom_name_name_length(reply));
            res = QString::fromLocal8Bit(replyText);
            free(reply);
        } else {
            // Log error
            qWarning() << "Failed to get atom name: " << error->error_code;
        }
        return res;
    }

    QString XcbKeyboardBackend::atomName(xcb_atom_t atom) const {
        return atomName(xcb_get_atom_name(m_conn, atom));
    }

    uint8_t XcbKeyboardBackend::getIndicatorMask(uint8_t i) const {
        xcb_xkb_get_indicator_map_cookie_t cookie;
        xcb_xkb_get_indicator_map_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;
        uint8_t mask = 0;

        cookie = xcb_xkb_get_indicator_map(m_conn, XCB_XKB_ID_USE_CORE_KBD, 1 << i);
        reply = xcb_xkb_get_indicator_map_reply(m_conn, cookie, &error);


        if (reply) {
            xcb_xkb_indicator_map_t *map = xcb_xkb_get_indicator_map_maps(reply);

            mask = map->mods;

            free(reply);
        } else {
            // Log error
            qWarning() << "Can't get indicator mask " << error->error_code;
        }
        return mask;
    }

    QList<QString> XcbKeyboardBackend::parseShortNames(QString text) {
        QRegularExpression re(QStringLiteral(R"(\+([a-z]+))"), QRegularExpression::CaseInsensitiveOption);

        QList<QString> res;
        QSet<QString> blackList; // blacklist wrong tokens
        blackList << QStringLiteral("inet") << QStringLiteral("group");

        // Loop through matched substrings
        int pos = 0;
        QRegularExpressionMatch match;
        while ((match = re.match(text, pos)).hasMatch()) {
            if (!blackList.contains(match.captured(1)))
                res << match.captured(1);
            pos += match.capturedLength();
        }
        return res;
    }

    void XcbKeyboardBackend::dispatchEvents() {
        // Pool events
        while (xcb_generic_event_t *event = xcb_poll_for_event(m_conn)) {
            // Check event types
            if (event->response_type != 0 && event->pad0 == XCB_XKB_STATE_NOTIFY) {
                xcb_xkb_state_notify_event_t *e = (xcb_xkb_state_notify_event_t *)event;

                // Update state
                d->capslock.enabled = e->lockedMods & d->capslock.mask;
                d->numlock.enabled  = e->lockedMods & d->numlock.mask;

                d->layout_id = e->group;
            } else if (event->response_type != 0 && event->pad0 == XCB_XKB_NEW_KEYBOARD_NOTIFY) {
                // Keyboards changed, reinit layouts
                initLayouts();
            }
            free(event);
        }
    }

    void XcbKeyboardBackend::connectEventsDispatcher(KeyboardModel *model) {
        // Setup events filter
        xcb_void_cookie_t cookie;
        xcb_xkb_select_events_details_t foo = {};
        xcb_generic_error_t *error = nullptr;

        cookie = xcb_xkb_select_events(m_conn, XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY, 0,
                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY, 0, 0, &foo);
        // Check errors
        error = xcb_request_check(m_conn, cookie);
        if (error) {
            qCritical() << "Can't select xck-xkb events: " << error->error_code;
            d->enabled = false;
            return;
        }

        // Flush connection
        xcb_flush(m_conn);

        // Get file descripor and init socket listener
        int fd = xcb_get_file_descriptor(m_conn);
        m_socket = new QSocketNotifier(fd, QSocketNotifier::Read);

        QObject::connect(m_socket, SIGNAL(activated(int)), model, SLOT(dispatchEvents()));
    }
}
