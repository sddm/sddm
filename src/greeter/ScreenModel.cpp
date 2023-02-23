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

#include "ScreenModel.h"

#include <memory>

#include <QGuiApplication>
#include <QScreen>

namespace SDDM {
    class ScreenModelPrivate {
    public:
        QScreen *screen { nullptr };
    };

    ScreenModel::ScreenModel(QScreen *screen, QObject *parent) : QAbstractListModel(parent), d(new ScreenModelPrivate()) {
        d->screen = screen;
    }

    ScreenModel::~ScreenModel() {
        delete d;
    }

    QHash<int, QByteArray> ScreenModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = QByteArrayLiteral("name");
        roleNames[GeometryRole] = QByteArrayLiteral("geometry");
        return roleNames;
    }

    int ScreenModel::primary() const {
        // This used to return the index of the primary screen, since this model
        // always have just one screen it should return 0 if it's primary or -1.
        return d->screen == QGuiApplication::primaryScreen() ? 0 : -1;
    }

    const QRect ScreenModel::geometry(int index) const {
        Q_UNUSED(index);
        return QRect(QPoint(0, 0), d->screen->geometry().size());
    }

    int ScreenModel::rowCount(const QModelIndex &parent) const {
        return parent.isValid() ? 0 : 1;
    }

    QVariant ScreenModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() >= 1)
            return QVariant();

        // return correct value
        if (role == NameRole)
            return d->screen->name();
        if (role == GeometryRole)
            return QRect(QPoint(0, 0), d->screen->geometry().size());

        // return empty value
        return QVariant();
    }
}
