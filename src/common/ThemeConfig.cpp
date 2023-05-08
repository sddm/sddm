/***************************************************************************
* Copyright (c) 2023 Fabian Vogt <fabian@ritter-vogt.de>
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
    ThemeConfig::ThemeConfig(const QString &path, QObject *parent)
        : QQmlPropertyMap(this, parent) {
        setTo(path);
    }

    void ThemeConfig::setTo(const QString &path) {
        for(const QString &key : keys()) {
            clear(key);
        }

        if (path.isNull()) {
            qDebug() << "Loaded empty theme configuration";
            return;
        }

        qDebug() << "Loading theme configuration from" << path;

        QSettings settings(path, QSettings::IniFormat);
        QSettings userSettings(path + QStringLiteral(".user"), QSettings::IniFormat);

        // Support non-latin strings in background picture path
        // Warning: The codec must be set immediately after creating the QSettings object,
        // before accessing any data.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
        userSettings.setIniCodec("UTF-8");
#endif

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

    QVariant ThemeConfig::value(const QString &key, const QVariant &def) {
        if (!contains(key)) {
            return def;
        }

        return value(key);
    }

    bool ThemeConfig::boolValue(const QString &key) {
        return value(key).toBool();
    }

    int ThemeConfig::intValue(const QString &key) {
        bool ok;
        auto ret = value(key).toInt(&ok);
        if (!ok) {
            qWarning() << "Could not convert" << key << "(value" << value(key) << ") to int";
        }
        return ret;
    }

    qreal ThemeConfig::realValue(const QString &key) {
        bool ok;
        auto ret = value(key).toReal(&ok);
        if (!ok) {
            qWarning() << "Could not convert" << key << "(value" << value(key) << ") to real";
        }
        return ret;
    }

    QString ThemeConfig::stringValue(const QString &key) {
        return value(key).toString();
    }
}
