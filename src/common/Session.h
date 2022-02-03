/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QDataStream>
#include <QDir>
#include <QSharedPointer>
#include <QProcessEnvironment>

namespace SDDM {
    class SessionModel;

    class Session {
    public:
        enum Type {
            UnknownSession = 0,
            X11Session,
            WaylandSession
        };

        explicit Session();
        Session(Type type, const QString &fileName);

        bool isValid() const;

        Type type() const;

        int vt() const;
        void setVt(int vt);

        QString xdgSessionType() const;

        QDir directory() const;
        QString fileName() const;

        QString displayName() const;
        QString comment() const;

        QString exec() const;
        QString tryExec() const;

        QString desktopSession() const;
        QString desktopNames() const;

        bool isHidden() const;
        bool isNoDisplay() const;

        QProcessEnvironment additionalEnv() const;

        void setTo(Type type, const QString &name);

        Session &operator=(const Session &other);

    private:
        QProcessEnvironment parseEnv(const QString &list);
        bool m_valid;
        Type m_type;
        int m_vt = 0;
        QDir m_dir;
        QString m_name;
        QString m_fileName;
        QString m_displayName;
        QString m_comment;
        QString m_exec;
        QString m_tryExec;
        QString m_xdgSessionType;
        QString m_desktopNames;
        QProcessEnvironment m_additionalEnv;
        bool m_isHidden;
        bool m_isNoDisplay;

        friend class SessionModel;
    };

    inline QDataStream &operator<<(QDataStream &stream, const Session &session) {
        stream << quint32(session.type()) << session.fileName();
        return stream;
    }

    inline QDataStream &operator>>(QDataStream &stream, Session &session) {
        quint32 type;
        QString fileName;
        stream >> type >> fileName;
        session.setTo(static_cast<Session::Type>(type), fileName);
        return stream;
    }
}

#endif // SDDM_SESSION_H
