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

#include "QAuthRequest.h"
#include "QAuth.h"
#include "QAuthMessages.h"

class QAuthRequest::Private : public QObject {
    Q_OBJECT
public slots:
    void responseChanged();
public:
    Private(QObject *parent);
    QList<QAuthPrompt*> prompts { };
    bool finishAutomatically { false };
    bool finished { true };
};

QAuthRequest::Private::Private(QObject* parent)
        : QObject(parent) { }

void QAuthRequest::Private::responseChanged() {
    Q_FOREACH(QAuthPrompt *qap, prompts) {
        if (qap->response().isEmpty())
            return;
    }
    if (finishAutomatically && prompts.length() > 0)
        qobject_cast<QAuthRequest*>(parent())->done();
}

QAuthRequest::QAuthRequest(QAuth *parent)
        : QObject(parent)
        , d(new Private(this)) { }

void QAuthRequest::setRequest(const Request *request) {
    QList<QAuthPrompt*> promptsCopy(d->prompts);
    d->prompts.clear();
    if (request != nullptr) {
        Q_FOREACH (const Prompt& p, request->prompts) {
            QAuthPrompt *qap = new QAuthPrompt(&p, this);
            d->prompts << qap;
            if (finishAutomatically())
                connect(qap, SIGNAL(responseChanged()), d, SLOT(responseChanged()));
        }
        d->finished = false;
    }
    Q_EMIT promptsChanged();
    if (request == nullptr) {
        qDeleteAll(promptsCopy);
    }
}

QList<QAuthPrompt*> QAuthRequest::prompts() {
    return d->prompts;
}

QQmlListProperty<QAuthPrompt> QAuthRequest::promptsDecl() {
    return QQmlListProperty<QAuthPrompt>(this, d->prompts);
}

void QAuthRequest::done() {
    if (!d->finished) {
        d->finished = true;
        Q_EMIT finished();
    }
}

bool QAuthRequest::finishAutomatically() {
    return d->finishAutomatically;
}

void QAuthRequest::setFinishAutomatically(bool value) {
    if (value != d->finishAutomatically) {
        d->finishAutomatically = value;
        Q_EMIT finishAutomaticallyChanged();
    }
}

Request QAuthRequest::request() const {
    Request r;
    Q_FOREACH (const QAuthPrompt* qap, d->prompts) {
        Prompt p;
        p.hidden = qap->hidden();
        p.message = qap->message();
        p.response = qap->response();
        p.type = qap->type();
        r.prompts << p;
    }
    return r;
}

#include "QAuthRequest.moc"
