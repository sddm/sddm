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

#ifndef SDE_USERMODEL_H
#define SDE_USERMODEL_H

#include <QAbstractListModel>
#include <QHash>

namespace SDE {
    class UserModelPrivate;

    class UserModel : public QAbstractListModel {
        Q_OBJECT
    public:
        enum UserRoles {
            UserNameRole = Qt::UserRole + 1,
            RealNameRole,
            HomeDirRole,
            IconRole
        };

        UserModel(QObject *parent = 0);
        ~UserModel();

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QHash<int, QByteArray> roleNames() const override;
#endif

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    private:
        UserModelPrivate *d { nullptr };
    };
}

#endif // SDE_USERMODEL_H
