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
    class Screen {
    public:
        QString name;
        QRect geometry;
    };

    typedef std::shared_ptr<Screen> ScreenPtr;

    class ScreenModelPrivate {
    public:
        QList<ScreenPtr> screens;
        QRect geometry;
        int primary { 0 };
    };

    ScreenModel::ScreenModel(QObject *parent) : QAbstractListModel(parent), d(new ScreenModelPrivate()) {
        connect(QGuiApplication::instance(), SIGNAL(screenAdded(QScreen*)), this, SLOT(onScreenAdded(QScreen*)));
        initScreens(true);
    }

    ScreenModel::~ScreenModel() {
        delete d;
    }

    QHash<int, QByteArray> ScreenModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = "name";
        roleNames[GeometryRole] = "geometry";
        return roleNames;
    }

    int ScreenModel::primary() const {
        return d->primary;
    }

    const QRect ScreenModel::geometry(int index) const {
        // return total geometry
        if (index == -1)
            return d->geometry;

        if (index < 0 || index >= d->screens.count())
            return QRect();

        return d->screens.at(index)->geometry;
    }

    void ScreenModel::onScreenAdded(QScreen *scrn) {
        // Recive screen updates
        connect(scrn, SIGNAL(geometryChanged(const QRect &)), this, SLOT(onScreenChanged()));
        onScreenChanged();
    }

    void ScreenModel::onScreenChanged() {
        initScreens(false);
    }

    int ScreenModel::rowCount(const QModelIndex &parent) const {
        return d->screens.length();
    }

    QVariant ScreenModel::data(const QModelIndex &index, int role) const {
        if (index.row() < 0 || index.row() >= d->screens.count())
            return QVariant();

        // get screen
        ScreenPtr screen = d->screens[index.row()];

        // return correct value
        if (role == NameRole)
            return screen->name;
        if (role == GeometryRole)
            return screen->geometry;

        // return empty value
        return QVariant();
    }

    void ScreenModel::initScreens(bool first) {
        // Clear
        beginResetModel();
        d->geometry = QRect();
        d->primary = 0;
        d->screens.clear();

#if 0
        // fake model for testing
        d->geometry = QRect(0, 0, 1920, 1080);
        d->primary = 1;
        d->screens << ScreenPtr { new Screen { "First", QRect(0, 0, 300, 300) } }
                   << ScreenPtr { new Screen { "Second", QRect(300, 0, 1320, 742) } }
                   << ScreenPtr { new Screen { "Third", QRect(1620, 0, 300, 300) } };
        return;
#endif

        QScreen *primaryScreen = QGuiApplication::primaryScreen();
        QList<QScreen *> screens = QGuiApplication::screens();
        for (int i = 0; i < screens.size(); ++i) {
            QScreen *screen = screens.at(i);
            // heuristic to detect clone mode, in that case only consider the primary screen
            if (screen->virtualGeometry() == primaryScreen->geometry() && screen != primaryScreen)
                continue;
            // add to the screens list
            d->screens << ScreenPtr { new Screen { QStringLiteral("Screen %1").arg(i + 1), screen->geometry() } };
            // extend available geometry
            d->geometry = d->geometry.united(screen->geometry());
            // check if primary
            if (screen == QGuiApplication::primaryScreen())
                d->primary = i;

            if (first) {
                // Recive screen updates
                connect(screen, SIGNAL(geometryChanged(const QRect &)), this, SLOT(onScreenChanged()));
            }
        }
        endResetModel();

        emit primaryChanged();
    }

}
