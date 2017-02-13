/*
 * PAM request and message IDs shared between greeter, daemon, helper
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

#ifndef AUTHBASE_H
#define AUTHBASE_H

#include <QtCore/QDataStream>

// AuthPrompt::Type
#include "AuthPrompt.h"

namespace SDDM {
    class Prompt {
    public:
        Prompt() { }
        Prompt(AuthPrompt::Type type, QString message, bool hidden)
                : type(type), message(message), hidden(hidden) { }
        Prompt(const Prompt &o)
                : type(o.type), response(o.response), message(o.message), hidden(o.hidden) { }
        ~Prompt() {
            clear();
        }
        Prompt& operator=(const Prompt &o) {
            type = o.type;
            response = o.response;
            message = o.message;
            hidden = o.hidden;
            return *this;
        }
        bool operator==(const Prompt &o) const {
            return type == o.type && response == o.response && message == o.message && hidden == o.hidden;
        }
        bool valid() const {
            return !(type == AuthPrompt::NONE && response.isEmpty() && message.isEmpty());
        }
        void clear() {
            type = AuthPrompt::NONE;
            // overwrite the whole thing with zeroes before clearing
            memset(response.data(), 0, response.length());
            response.clear();
            message.clear();
            hidden = false;
        }

        AuthPrompt::Type type { AuthPrompt::NONE };
        QByteArray response { };
        QString message { };
        bool hidden { false };
    };

    class Request {
    public:
        Request() { }
        Request(QList<Prompt> prompts)
                : prompts(prompts) { }
        Request(const Request &o)
                : prompts(o.prompts) { }
        Request& operator=(const Request &o) {
            prompts = QList<Prompt>(o.prompts);
            return *this;
        }
        bool operator==(const Request &o) const {
            return prompts == o.prompts;
        }
        bool valid() const {
            return !(prompts.isEmpty());
        }
        void clear() {
            prompts.clear();
        }

        QList<Prompt> prompts { };
    };

    inline QDataStream& operator<<(QDataStream &s, const Prompt &m) {
        s << qint32(m.type) << m.message << m.hidden << m.response;
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, Prompt &m) {
        qint32 type;
        QString message;
        bool hidden;
        QByteArray response;
        s >> type >> message >> hidden >> response;
        m.type = AuthPrompt::Type(type);
        m.message = message;
        m.hidden = hidden;
        m.response = response;
        return s;
    }

    inline QDataStream& operator<<(QDataStream &s, const Request &m) {
        qint32 length = m.prompts.length();
        s << length;
        for(const Prompt &p : qAsConst(m.prompts)) {
            s << p;
        }
        return s;
    }

    inline QDataStream& operator>>(QDataStream &s, Request &m) {
        QList<Prompt> prompts;
        qint32 length;
        s >> length;
        for (int i = 0; i < length; i++) {
            Prompt p;
            s >> p;
            prompts << p;
        }
        if (prompts.length() != length) {
            s.setStatus(QDataStream::ReadCorruptData);
            return s;
        }
        m.prompts = prompts;
        return s;
    }
}

#endif // AUTHBASE_H
