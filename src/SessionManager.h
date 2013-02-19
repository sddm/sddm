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

#ifndef SDE_SESSIONMANAGER_H
#define SDE_SESSIONMANAGER_H

#include <QObject>
#include <QString>

namespace SDE {
    class SessionManagerPrivate;

    class SessionManager : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString hostName READ hostName CONSTANT)
    public:
        SessionManager();
        ~SessionManager();

        const QString &hostName() const;

        void setCookie(const char *cookie);
        void setDisplay(const QString &displayName);

        void autoLogin();

    public slots:
        void login(const QString &username, const QString &password, const int sessionIndex);

    signals:
        void fail();
        void success();

    private:
        SessionManagerPrivate *d { nullptr };
    };
}
#endif // SDE_SESSIONMANAGER_H
