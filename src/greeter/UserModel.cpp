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

#include "Constants.h"
#include "Configuration.h"

#include <QFile>
#include <QList>
#include <QTextStream>
#include <QStringList>

#include <memory>
#include <pwd.h>

namespace SDDM {
    class User {
    public:
        QString name { "" };
        QString realName { "" };
        QString homeDir { "" };
        QString icon { "" };
        int uid { 0 };
        int gid { 0 };
    };

    typedef std::shared_ptr<User> UserPtr;

    class UserModelPrivate {
    public:
        int lastIndex { 0 };
        QList<UserPtr> users;
    };

    UserModel::UserModel(QObject *parent) : QAbstractListModel(parent), d(new UserModelPrivate()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = "name";
        roleNames[RealNameRole] = "realName";
        roleNames[HomeDirRole] = "homeDir";
        roleNames[IconRole] = "icon";
        // set role names
        setRoleNames(roleNames);
#endif
        struct passwd *current_pw;
        while ((current_pw = getpwent()) != nullptr) {

            // skip entries with uids smaller than minimum uid
            if ( int(current_pw->pw_uid) < Configuration::instance()->minimumUid())
                continue;

            // skip entries with uids greater than maximum uid
            if ( int(current_pw->pw_uid) > Configuration::instance()->maximumUid())
                continue;
            // skip entries with user names in the hide users list
            if (Configuration::instance()->hideUsers().contains(current_pw->pw_name))
                continue;

            // skip entries with shells in the hide shells list
            if (Configuration::instance()->hideShells().contains(current_pw->pw_shell))
                continue;

            // create user
            UserPtr user { new User() };
            user->name = QString(current_pw->pw_name);
            user->realName = QString::fromLatin1(current_pw->pw_gecos).split(",").first();
            user->homeDir = QString(current_pw->pw_dir);
            user->uid = int(current_pw->pw_uid);
            user->gid = int(current_pw->pw_gid);

            // search for face icon
            QString userFace = QString("%1/.face.icon").arg(user->homeDir);
            QString systemFace = QString("%1/%2.face.icon").arg(Configuration::instance()->facesDir()).arg(user->name);
            if (QFile::exists(userFace))
                user->icon = userFace;
            else if (QFile::exists(systemFace))
                user->icon = systemFace;
            else
                user->icon = QString("%1/default.face.icon").arg(Configuration::instance()->facesDir());

            // add user
            d->users << user;
        }

        endpwent();

        // sort users by username
        std::sort(d->users.begin(), d->users.end(), [&](const UserPtr &u1, const UserPtr &u2) { return u1->name < u2->name; });

        // find out index of the last user
        for (int i = 0; i < d->users.size(); ++i) {
            if (d->users.at(i)->name == Configuration::instance()->lastUser())
                d->lastIndex = i;
        }
    }

    UserModel::~UserModel() {
        delete d;
    }

#ifdef USE_QT5
    QHash<int, QByteArray> UserModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = "name";
        roleNames[RealNameRole] = "realName";
        roleNames[HomeDirRole] = "homeDir";
        roleNames[IconRole] = "icon";

        return roleNames;
    }
#endif

    const int UserModel::lastIndex() const {
        return d->lastIndex;
    }

    const QString &UserModel::lastUser() const {
        return Configuration::instance()->lastUser();
    }

    int UserModel::rowCount(const QModelIndex &parent) const {
        return d->users.length();
    }

    QVariant UserModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() > d->users.count())
            return QVariant();

        // get user
        UserPtr user = d->users[index.row()];

        // return correct value
        if (role == NameRole)
            return user->name;
        else if (role == RealNameRole)
            return user->realName;
        else if (role == HomeDirRole)
            return user->homeDir;
        else if (role == IconRole)
            return user->icon;

        // return empty value
        return QVariant();
    }
}
