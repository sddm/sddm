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

#include "UserModel.h"

#include "Configuration.h"

#include <QFile>
#include <QList>
#include <QTextStream>

#include <memory>

namespace SDE {
    class User {
    public:
        QString userName { "" };
        QString realName { "" };
        QString homeDir { "" };
        QString icon { "" };
        int uid { 0 };
        int gid { 0 };
    };

    typedef std::shared_ptr<User> UserPtr;

    class UserModelPrivate {
    public:
        QList<UserPtr> users;
    };

    UserModel::UserModel(QObject *parent) : QAbstractListModel(parent), d(new UserModelPrivate()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[Qt::UserRole + 1] = "userName";
        roleNames[Qt::UserRole + 2] = "realName";
        roleNames[Qt::UserRole + 3] = "homeDir";
        roleNames[Qt::UserRole + 4] = "icon";
        // set role names
        setRoleNames(roleNames);
#endif

        // create file object
        QFile file("/etc/passwd");
        // open file
        if (!file.open(QIODevice::ReadOnly))
            return;

        // create text stream
        QTextStream in(&file);

        // process lines
        while (!in.atEnd()) {

            // read line
            QString line = in.readLine();

            // split line into fields
            QStringList fields = line.split(":", QString::KeepEmptyParts);

            // there should be exactly 7 fields
            if (fields.length() != 7)
                continue;

            // skip entries with uids smaller than minimum uid
            if (fields.at(2).toInt() < Configuration::instance()->minimumUid())
                continue;

            // create user
            UserPtr user { new User() };
            user->userName = fields.at(0);
            user->realName = fields.at(4).split(",").first();
            user->homeDir = fields.at(5);
            user->uid = fields.at(2).toInt();
            user->gid = fields.at(3).toInt();

            // search for face icon
            QString userFace = QString("%1/.face.icon").arg(user->homeDir);
            QString systemFace = QString("%1/%2.face.icon").arg(Configuration::instance()->facesDir()).arg(user->userName);
            if (QFile::exists(userFace))
                user->icon = userFace;
            else if (QFile::exists(systemFace))
                user->icon = systemFace;
            else
                user->icon = QString("%1/default.face.icon").arg(Configuration::instance()->facesDir());

            // add user
            d->users << user;
        }

        // close file
        file.close();

        // sort users by username
        std::sort(d->users.begin(), d->users.end(), [&](const UserPtr &u1, const UserPtr &u2) { return u1->userName < u2->userName; });
    }

    UserModel::~UserModel() {
        delete d;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QHash<int, QByteArray> UserModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[Qt::UserRole + 1] = "userName";
        roleNames[Qt::UserRole + 2] = "realName";
        roleNames[Qt::UserRole + 3] = "homeDir";
        roleNames[Qt::UserRole + 4] = "icon";

        return roleNames;
    }
#endif

    int UserModel::rowCount(const QModelIndex &parent) const {
        return d->users.length();
    }

    QVariant UserModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() > d->users.count())
            return QVariant();

        // get user
        UserPtr user = d->users[index.row()];

        // return correct value
        if (role == (Qt::UserRole + 1))
            return user->userName;
        else if (role == (Qt::UserRole + 2))
            return user->realName;
        else if (role == (Qt::UserRole + 3))
            return user->homeDir;
        else if (role == (Qt::UserRole + 4))
            return user->icon;

        // return empty value
        return QVariant();
    }
}
