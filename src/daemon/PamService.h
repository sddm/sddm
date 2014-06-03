/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#ifndef SDDM_PAMSERVICE_H
#define SDDM_PAMSERVICE_H

#include <QProcess>
#include <QString>

#include <security/pam_appl.h>

namespace SDDM {
    class Display;

    class PamService {
    public:
        PamService(const char *service, const QString &user, const QString &password, bool passwordless);
        ~PamService();

        Display *display() const;
        void setDisplay(Display *display);

        QString sessionClass() const;
        void setSessionClass(const QString &sessionClass);

        QString sessionType() const;
        void setSessionType(const QString &sessionType);

        QString sessionDesktop() const;
        void setSessionDesktop(const QString &sessionDesktop);

        QProcessEnvironment systemEnvironment() const;

        bool authenticate(char **mapped);

        QString user { "" };
        QString password { "" };
        bool passwordless { false };

    private:
        struct pam_conv m_converse;
        pam_handle_t *m_handle { nullptr };
        int m_result { PAM_SUCCESS };

        QString m_sessionClass { "" };
        QString m_sessionType { "" };
        QString m_sessionDesktop { "" };

        Display *m_display { nullptr };
    };
}

#endif // SDDM_PAMSERVICE_H
