/*
 * /etc/passwd authentication backend
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#if !defined(PASSWDBACKEND_H) && !defined(USE_PAM)
#define PASSWDBACKEND_H

#include "../Backend.h"

namespace SDDM {
    class PasswdBackend : public Backend {
        Q_OBJECT
    public:
        PasswdBackend(HelperApp *parent);

    public slots:
        virtual bool start(const QString &user = QString());
        virtual bool authenticate();

        virtual QString userName();

    private:
        QString m_user { };
    };
}

#endif // PASSWDBACKEND_H
