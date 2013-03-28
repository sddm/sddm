/***************************************************************************
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

#ifndef SDDM_SESSION_H
#define SDDM_SESSION_H

#include <QDBusObjectPath>
#include <QProcess>
#include <QString>

namespace SDDM {
    class Session : public QProcess {
        Q_OBJECT
        Q_DISABLE_COPY(Session)
        Q_PROPERTY(QDBusObjectPath Seat READ seatPath CONSTANT)
        Q_PROPERTY(QString UserName READ user CONSTANT)
    public:
        explicit Session(const QString &name, QObject *parent = 0);

        const QString &name() const;
        const QString &path() const;

        const QString &seat() const;
        void setSeat(const QString &seat);

        const QString &user() const;
        void setUser(const QString &user);

        void setDir(const QString &dir);
        void setUid(int uid);
        void setGid(int gid);

        const QDBusObjectPath &seatPath() const;

        void Lock();

    protected:
        void setupChildProcess();

    private:
        QString m_name { "" };
        QString m_path { "" };
        QString m_seat { "" };
        QDBusObjectPath m_seatPath { };
        QString m_user { "" };
        QString m_dir { "" };
        int m_uid { 0 };
        int m_gid { 0 };
    };
}

#endif // SDDM_SESSION_H
