/*
 * Qt Authentication Library
 * Copyright (c) 2013 Martin Bříza <mbriza@redhat.com>
 * Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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

#include "AuthBase.h"
#include "AuthRequest.h"
#include "AuthMessages.h"

namespace SDDM {
    class AuthRequest::Private : public QObject {
        Q_OBJECT
    public slots:
        void responseChanged();
    public:
        Private(QObject *parent);
        QList<AuthPrompt*> prompts { };
        bool finishAutomatically { false };
        bool finished { true };
    };

    AuthRequest::Private::Private(QObject* parent)
            : QObject(parent) { }

    void AuthRequest::Private::responseChanged() {
        for(const AuthPrompt *qap : qAsConst(prompts)) {
            if (qap->response().isEmpty())
                return;
        }
        if (finishAutomatically && prompts.length() > 0)
            qobject_cast<AuthRequest*>(parent())->done();
    }

    AuthRequest::AuthRequest(QObject *parent)
            : QObject(parent)
            , d(new Private(this)) { }

    void AuthRequest::setRequest(const Request * const request) {
        QList<AuthPrompt*> promptsCopy(d->prompts);
        d->prompts.clear();
        if (request != nullptr) {
            for(const Prompt& p : qAsConst(request->prompts)) {
                AuthPrompt *qap = new AuthPrompt(&p, this);
                d->prompts << qap;
                if (finishAutomatically())
                    connect(qap, &AuthPrompt::responseChanged, d, &AuthRequest::Private::responseChanged);
            }
            d->finished = false;
        }
        Q_EMIT promptsChanged();
        if (request == nullptr) {
            qDeleteAll(promptsCopy);
        }
    }

    QList<AuthPrompt*> AuthRequest::prompts() {
        return d->prompts;
    }

    QQmlListProperty<AuthPrompt> AuthRequest::promptsRead() {
        return QQmlListProperty<AuthPrompt>(this, d->prompts);
    }

    void AuthRequest::done() {
        if (!d->finished) {
            d->finished = true;
            Q_EMIT finished();
        }
    }

    // send to (pam) backend
    void AuthRequest::cancel() {
        if (!d->finished) {
            d->finished = true;
        }
        Q_EMIT canceled();
    }

    bool AuthRequest::finishAutomatically() {
        return d->finishAutomatically;
    }

    void AuthRequest::setFinishAutomatically(bool value) {
        if (value != d->finishAutomatically) {
            d->finishAutomatically = value;
            Q_EMIT finishAutomaticallyChanged();
        }
    }

    Request AuthRequest::request() const {
        Request r;
        for(const AuthPrompt* qap : qAsConst(d->prompts)) {
            Prompt p;
            p.hidden = qap->hidden();
            p.message = qap->message();
            p.response = qap->response();
            p.type = qap->type();
            r.prompts << p;
        }
        return r;
    }

    QString AuthRequest::findChangePwdMessage() {
        for(const AuthPrompt* qap : qAsConst(d->prompts)) {
            if(qap->type()==AuthPrompt::CHANGE_PASSWORD)
                return qap->message();
        }
        return QString();
    }

    AuthPrompt *AuthRequest::findPrompt(AuthPrompt::Type type) const {
        for(AuthPrompt* qap : qAsConst(d->prompts)) {
            if(qap->type()==type)
                return qap;
        }
        return NULL;
    }

    bool AuthRequest::setLoginResponse(const QString &username, const QString &password) {
        AuthPrompt* prompt = nullptr;

        if(!username.isNull()) {
            prompt = findPrompt(AuthPrompt::LOGIN_USER);
            if(prompt) prompt->setResponse(qPrintable(username));
        }
        if(!password.isNull()) {
            prompt = findPrompt(AuthPrompt::LOGIN_PASSWORD);
            if(prompt) prompt->setResponse(qPrintable(password));
        }
        if(prompt)
            return true;

        return false;
    }

    bool AuthRequest::setChangeResponse(const QString &password) {
        AuthPrompt* prompt;

        if(!password.isNull()) {
            prompt = findPrompt(AuthPrompt::CHANGE_PASSWORD);
            if(prompt) {
                prompt->setResponse(qPrintable(password));
                return true;
            }
        }

        return false;
    }
}

#include "AuthRequest.moc"
