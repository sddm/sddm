/*
 * Base backend class to be inherited further
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

#ifndef BACKEND_H
#define BACKEND_H

#include <QtCore/QObject>

namespace SDDM {
    class HelperApp;
    class Backend : public QObject
    {
        Q_OBJECT
    public:
        /**
        * Requests allocation of a new backend instance.
        * The method chooses the most suitable one for the current system.
        */
        static Backend *get(HelperApp *parent);

        void setAutologin(bool on = true);
        void setDisplayServer(bool on = true);
        void setGreeter(bool on = true);

    public slots:
        virtual bool start(const QString &user = QString()) = 0;
        virtual bool authenticate() = 0;
        virtual bool openSession();
        virtual bool closeSession();

        virtual QString userName() = 0;

    protected:
        Backend(HelperApp *parent);
        HelperApp *m_app;
        bool m_autologin { false };
        bool m_displayServer = false;
        bool m_greeter { false };
    };
}

#endif // BACKEND_H
