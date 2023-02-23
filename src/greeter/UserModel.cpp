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
        User(const struct passwd *data, const QString icon) :
            name(QString::fromLocal8Bit(data->pw_name)),
            realName(QString::fromLocal8Bit(data->pw_gecos).split(QLatin1Char(',')).first()),
            homeDir(QString::fromLocal8Bit(data->pw_dir)),
            uid(data->pw_uid),
            gid(data->pw_gid),
            // if shadow is used pw_passwd will be 'x' nevertheless, so this
            // will always be true
            needsPassword(strcmp(data->pw_passwd, "") != 0),
            icon(icon)
        {}

        QString name;
        QString realName;
        QString homeDir;
        int uid { 0 };
        int gid { 0 };
        bool needsPassword { false };
        QString icon;
    };

    typedef std::shared_ptr<User> UserPtr;

    class UserModelPrivate {
    public:
        int lastIndex { 0 };
        QList<UserPtr> users;
        bool containsAllUsers { true };
    };

    UserModel::UserModel(bool needAllUsers, QObject *parent) : QAbstractListModel(parent), d(new UserModelPrivate()) {
        const QString facesDir = mainConfig.Theme.FacesDir.get();
        const QString themeDir = mainConfig.Theme.ThemeDir.get();
        const QString currentTheme = mainConfig.Theme.Current.get();
        const QString themeDefaultFace = QStringLiteral("%1/%2/faces/.face.icon").arg(themeDir).arg(currentTheme);
        const QString defaultFace = QStringLiteral("%1/.face.icon").arg(facesDir);
        const QString iconURI = QStringLiteral("file://%1").arg(
                QFile::exists(themeDefaultFace) ? themeDefaultFace : defaultFace);

        bool lastUserFound = false;

        struct passwd *current_pw;
        setpwent();
        while ((current_pw = getpwent()) != nullptr) {

            // skip entries with uids smaller than minimum uid
            if (int(current_pw->pw_uid) < mainConfig.Users.MinimumUid.get())
                continue;

            // skip entries with uids greater than maximum uid
            if (int(current_pw->pw_uid) > mainConfig.Users.MaximumUid.get())
                continue;
            // skip entries with user names in the hide users list
            if (mainConfig.Users.HideUsers.get().contains(QString::fromLocal8Bit(current_pw->pw_name)))
                continue;

            // skip entries with shells in the hide shells list
            if (mainConfig.Users.HideShells.get().contains(QString::fromLocal8Bit(current_pw->pw_shell)))
                continue;

            // create user
            UserPtr user { new User(current_pw, iconURI) };

            // add user
            d->users << user;

            if (user->name == lastUser())
                lastUserFound = true;

            if (!needAllUsers && d->users.count() > mainConfig.Theme.DisableAvatarsThreshold.get()) {
                struct passwd *lastUserData;
                // If the theme doesn't require that all users are present, try to add the data for lastUser at least
                if(!lastUserFound && (lastUserData = getpwnam(qPrintable(lastUser()))))
                    d->users << UserPtr(new User(lastUserData, themeDefaultFace));

                d->containsAllUsers = false;
                break;
            }
        }

        endpwent();

        // sort users by username
        std::sort(d->users.begin(), d->users.end(), [&](const UserPtr &u1, const UserPtr &u2) { return u1->name < u2->name; });
        // Remove duplicates in case we have several sources specified
        // in nsswitch.conf(5).
        auto newEnd = std::unique(d->users.begin(), d->users.end(), [&](const UserPtr &u1, const UserPtr &u2) { return u1->name == u2->name; });
        d->users.erase(newEnd, d->users.end());

        bool avatarsEnabled = mainConfig.Theme.EnableAvatars.get();
        if (avatarsEnabled && mainConfig.Theme.EnableAvatars.isDefault()) {
            if (d->users.count() > mainConfig.Theme.DisableAvatarsThreshold.get()) avatarsEnabled=false;
        }

        // find out index of the last user
        for (int i = 0; i < d->users.size(); ++i) {
            UserPtr user { d->users.at(i) };
            if (user->name == stateConfig.Last.User.get())
                d->lastIndex = i;

            if (avatarsEnabled) {
                const QString userFace = QStringLiteral("%1/.face.icon").arg(user->homeDir);
                const QString systemFace = QStringLiteral("%1/%2.face.icon").arg(facesDir).arg(user->name);
                const QString accountsServiceFace = QStringLiteral(ACCOUNTSSERVICE_DATA_DIR "/icons/%1").arg(user->name);

                QString userIcon;
                // If the home is encrypted it takes a lot of time to open
                // up the greeter, therefore we try the system avatar first
                if (QFile::exists(systemFace))
                    userIcon = systemFace;
                else if (QFile::exists(userFace))
                    userIcon = userFace;
                else if (QFile::exists(accountsServiceFace))
                    userIcon = accountsServiceFace;

                if (!userIcon.isEmpty())
                    user->icon = QStringLiteral("file://%1").arg(userIcon);
            }
        }
    }

    UserModel::~UserModel() {
        delete d;
    }

    QHash<int, QByteArray> UserModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = QByteArrayLiteral("name");
        roleNames[RealNameRole] = QByteArrayLiteral("realName");
        roleNames[HomeDirRole] = QByteArrayLiteral("homeDir");
        roleNames[IconRole] = QByteArrayLiteral("icon");
        roleNames[NeedsPasswordRole] = QByteArrayLiteral("needsPassword");

        return roleNames;
    }

    int UserModel::lastIndex() const {
        return d->lastIndex;
    }

    QString UserModel::lastUser() const {
        return stateConfig.Last.User.get();
    }

    int UserModel::rowCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : d->users.length();
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
        else if (role == NeedsPasswordRole)
            return user->needsPassword;

        // return empty value
        return QVariant();
    }

    int UserModel::disableAvatarsThreshold() const {
        return mainConfig.Theme.DisableAvatarsThreshold.get();
    }

    bool UserModel::containsAllUsers() const {
        return d->containsAllUsers;
    }
}
