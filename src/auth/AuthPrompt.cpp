/*
 * Qt Authentication Library
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include "AuthPrompt.h"
#include "Auth.h"
#include "AuthMessages.h"

namespace SDDM {
    class AuthPrompt::Private : public Prompt {
    public:
        Private(const Prompt *p) {
            // initializers are too mainstream i guess
            type = p->type;
            hidden = p->hidden;
            message = p->message;
            response = p->response;
        }
    };

    AuthPrompt::AuthPrompt(const Prompt *prompt, AuthRequest *parent)
            : QObject(parent)
            , d(new Private(prompt)) {
    }

    AuthPrompt::~AuthPrompt() {
        delete d;
    }

    AuthPrompt::Type AuthPrompt::type() const {
        return d->type;
    }

    QString AuthPrompt::message() const {
        return d->message;
    }

    QByteArray AuthPrompt::response() const {
        return d->response;
    }

    QByteArray AuthPrompt::responseFake() {
        return QByteArray();
    }

    void AuthPrompt::setResponse(const QByteArray &r) {
        if (r != d->response) {
            d->response = r;
            Q_EMIT responseChanged();
        }
    }

    bool AuthPrompt::hidden() const {
        return d->hidden;
    }
}
