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

#ifdef USE_QT5
#include <QGuiApplication>
#include <QScreen>
#else
#include <QApplication>
#include <QDesktopWidget>
#endif

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
#ifdef USE_QT5
        connect(QGuiApplication::instance(), SIGNAL(screenAdded(QScreen*)), this, SLOT(onScreenAdded(QScreen*)));
#else
        connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(onScreenChanged()));
        connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(onScreenChanged()));
#endif
        initScreens(true);
    }

    ScreenModel::~ScreenModel() {
        delete d;
    }

#ifdef USE_QT5
    QHash<int, QByteArray> ScreenModel::roleNames() const {
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = "name";
        roleNames[GeometryRole] = "geometry";
        return roleNames;
    }
#endif

    const int ScreenModel::primary() const {
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

#ifdef USE_QT5
    void ScreenModel::onScreenAdded(QScreen *scrn) {
        // Recive screen updates
        connect(scrn, SIGNAL(geometryChanged(const QRect &)), this, SLOT(onScreenChanged()));
        onScreenChanged();
    }
#endif

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
        d->geometry = QRect();
        d->primary = 0;
        d->screens.clear();

#ifndef USE_QT5
        // set role names
        QHash<int, QByteArray> roleNames;
        roleNames[NameRole] = "name";
        roleNames[GeometryRole] = "geometry";
        // set role names
        setRoleNames(roleNames);
#endif

#if 0
        // fake model for testing
        d->geometry = QRect(0, 0, 1920, 1080);
        d->primary = 1;
        d->screens << ScreenPtr { new Screen { "First", QRect(0, 0, 300, 300) } }
                   << ScreenPtr { new Screen { "Second", QRect(300, 0, 1320, 742) } }
                   << ScreenPtr { new Screen { "Third", QRect(1620, 0, 300, 300) } };
        return;
#endif

#ifdef USE_QT5
        QList<QScreen *> screens = QGuiApplication::screens();
        for (int i = 0; i < screens.size(); ++i) {
            QScreen *screen = screens.at(i);
            // add to the screens list
            d->screens << ScreenPtr { new Screen { QString("Screen %1").arg(i + 1), screen->geometry() } };
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
#else
        // set primary screen
        d->primary = QApplication::desktop()->primaryScreen();
        // get screen count
        int screenCount = QApplication::desktop()->screenCount();

        for (int i = 0; i < screenCount; ++i) {
            QRect geometry = QApplication::desktop()->screenGeometry(i);
            // add to the screens list
            d->screens << ScreenPtr { new Screen { QString("Screen %1").arg(i + 1), geometry } };
            // extend available geometry
            d->geometry = d->geometry.united(geometry);
        }
#endif
        emit screensChanged();
    }

}
