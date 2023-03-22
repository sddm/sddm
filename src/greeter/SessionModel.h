/***************************************************************************
* Copyright (c) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_SESSIONMODEL_H
#define SDDM_SESSIONMODEL_H

#include "Session.h"

#include <QAbstractListModel>

#include <QHash>

namespace SDDM {
    class SessionModelPrivate;

    class SessionModel : public QAbstractListModel {
        Q_OBJECT
        Q_DISABLE_COPY(SessionModel)
        Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)
    public:
        enum SessionRole {
            DirectoryRole = Qt::UserRole + 1,
            FileRole,
            TypeRole,
            NameRole,
            ExecRole,
            CommentRole
        };
        Q_ENUM(SessionRole)

        SessionModel(QObject *parent = 0);
        ~SessionModel();

        QHash<int, QByteArray> roleNames() const override;

        int lastIndex() const;

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    private:
        SessionModelPrivate *d { nullptr };

        void populate(Session::Type type, const QStringList &dirPaths);
    };
}

#endif // SDDM_SESSIONMODEL_H
