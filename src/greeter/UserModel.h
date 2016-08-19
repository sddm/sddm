/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
* Copyright (c) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
* Copyright (c) 2015 Mingye Wang (Arthur2e5) <arthur200126@gmail.com>
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

#ifndef SDDM_USERMODEL_H
#define SDDM_USERMODEL_H

#include <QAbstractListModel>

#include <QHash>

#if HAVE_QTACCOUNTSSERVICE
#include <QtAccountsService/AccountsManager>  /* QtAccountsService */
#endif

namespace SDDM {
    class UserModelPrivate;

    class UserModel : public QAbstractListModel {
        Q_OBJECT
        Q_DISABLE_COPY(UserModel)
        Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)
        Q_PROPERTY(QString lastUser READ lastUser CONSTANT)
        Q_PROPERTY(int count READ rowCount CONSTANT)
        Q_PROPERTY(int disableAvatarsThreshold READ disableAvatarsThreshold CONSTANT)
    public:
        enum UserRoles {
            NameRole = Qt::UserRole + 1,
            RealNameRole,
            HomeDirRole,
            IconRole,
            NeedsPasswordRole
        };

        UserModel(QObject *parent = 0);
        ~UserModel();

        QHash<int, QByteArray> roleNames() const override;

        const int lastIndex() const;
        QString lastUser() const;

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        int disableAvatarsThreshold() const;
    private:
        UserModelPrivate *d { nullptr };
#if HAVE_QTACCOUNTSSERVICE
        AccountsService::AccountsManager *asAccountsManager { nullptr };
#endif
    };
}

#endif // SDDM_USERMODEL_H
