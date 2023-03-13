/*
 * PAM authentication backend
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

#if !defined(PAMBACKEND_H)
#define PAMBACKEND_H

#include "Constants.h"
#include "AuthMessages.h"
#include "../Backend.h"

#include <QtCore/QObject>

#include <security/pam_appl.h>

namespace SDDM {
    class PamHandle;
    class PamBackend;
    class PamData {
    public:
        PamData();

        bool insertPrompt(const struct pam_message *msg, bool predict = true);
        Auth::Info handleInfo(const struct pam_message *msg, bool predict);

        const Request& getRequest() const;
        void completeRequest(const Request& request);

        QByteArray getResponse(const struct pam_message *msg);

    private:
        AuthPrompt::Type detectPrompt(const struct pam_message *msg) const;

        const Prompt& findPrompt(const struct pam_message *msg) const;
        Prompt& findPrompt(const struct pam_message *msg);

        bool m_sent { false };
        Request m_currentRequest { };
    };

    class PamBackend : public Backend
    {
        Q_OBJECT
    public:
        explicit PamBackend(HelperApp *parent);
        virtual ~PamBackend();
        int converse(int n, const struct pam_message **msg, struct pam_response **resp);

    public slots:
        virtual bool start(const QString &user = QString());
        virtual bool authenticate();
        virtual bool openSession();
        virtual bool closeSession();

        virtual QString userName();

    private:
        PamData *m_data { nullptr };
        PamHandle *m_pam { nullptr };
    };
}

#endif // PAMBACKEND_H
