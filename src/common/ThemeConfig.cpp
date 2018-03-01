/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2014 David Edmundson <davidedmundson@kde.org>
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

#include "ThemeConfig.h"

#include <QDebug>
#include <QSettings>
#include <QStringList>

namespace SDDM {
    ThemeConfig::ThemeConfig(const QString &path) {
        setTo(path);
    }

    void ThemeConfig::setTo(const QString &path) {
        clear();

        qDebug() << "Loading theme configuration from" << path;

        QSettings settings(path, QSettings::IniFormat);
        QSettings userSettings(path + QStringLiteral(".user"), QSettings::IniFormat);

        // Support non-latin strings in background picture path
        // Warning: The codec must be set immediately after creating the QSettings object,
        // before accessing any data.
        settings.setIniCodec("UTF-8");
        userSettings.setIniCodec("UTF-8");

        // read default keys
        for (const QString &key: settings.allKeys()) {
            insert(key, settings.value(key));
        }
        // read user set themes overwriting defaults if they exist
        for (const QString &key: userSettings.allKeys()) {
            if (!userSettings.value(key).toString().isEmpty()) {
                insert(key, userSettings.value(key));
            }
        }

        //if the main config contains a background, save this to a new config value
        //to themes can use it if the user set config background cannot be loaded
        if (settings.contains(QStringLiteral("background"))) {
            insert(QStringLiteral("defaultBackground"), settings.value(QStringLiteral("background")));
        }
    }
}
