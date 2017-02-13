/*
 * Message IDs to pass between the library and the helper
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <QtCore/QDataStream>
#include <QtCore/QProcessEnvironment>

#include "Auth.h"

// Request and Prompt classes
#include "AuthBase.h"

namespace SDDM {

    enum Msg {
        MSG_UNKNOWN = 0,
        HELLO = 1,
        ERROR,
        INFO,
        REQUEST,
        CANCEL,
        AUTHENTICATED,
        SESSION_STATUS,
        DISPLAY_SERVER_STARTED,
        MSG_LAST,
    };

    inline QDataStream& operator<<(QDataStream &s, const Msg &m) {
        s << qint32(m);
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, Msg &m) {
        // TODO seriously?
        qint32 i;
        s >> i;
        if (i >= MSG_LAST || i <= MSG_UNKNOWN) {
            s.setStatus(QDataStream::ReadCorruptData);
            return s;
        }
        m = Msg(i);
        return s;
    }

    inline QDataStream& operator<<(QDataStream &s, const AuthEnums::Error &m) {
        s << qint32(m);
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, AuthEnums::Error &m) {
        // TODO seriously?
        qint32 i;
        s >> i;
        if (i >= AuthEnums::_ERROR_LAST || i < AuthEnums::ERROR_NONE) {
            s.setStatus(QDataStream::ReadCorruptData);
            return s;
        }
        m = AuthEnums::Error(i);
        return s;
    }

    inline QDataStream& operator<<(QDataStream &s, const AuthEnums::Info &m) {
        s << qint32(m);
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, AuthEnums::Info &m) {
        // TODO seriously?
        qint32 i;
        s >> i;
        if (i >= AuthEnums::_INFO_LAST || i < AuthEnums::INFO_NONE) {
            s.setStatus(QDataStream::ReadCorruptData);
            return s;
        }
        m = AuthEnums::Info(i);
        return s;
    }

    inline QDataStream& operator<<(QDataStream &s, const QProcessEnvironment &m) {
        s << m.toStringList();
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, QProcessEnvironment &m) {
        QStringList l;
        s >> l;
        for (QString s : l) {
            int pos = s.indexOf(QLatin1Char('='));
            m.insert(s.left(pos), s.mid(pos + 1));
        }
        return s;
    }
}

#endif // MESSAGES_H
