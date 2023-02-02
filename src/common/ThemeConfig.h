/***************************************************************************
* Copyright (c) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_THEMECONFIG_H
#define SDDM_THEMECONFIG_H

#include <QQmlPropertyMap>

namespace SDDM {
    class ThemeConfig : public QQmlPropertyMap {
        Q_OBJECT
    public:
        explicit ThemeConfig(const QString &path, QObject *parent = nullptr);

        void setTo(const QString &path);

        // Also provide QVariantMap's value(key, default) method
        using QQmlPropertyMap::value;
        QVariant value(const QString &key, const QVariant &def);

        // QSettings::IniFormat returns string types for basic
        // types. Let the theme request specific conversions.
        Q_INVOKABLE bool boolValue(const QString &key);
        Q_INVOKABLE int intValue(const QString &key);
        Q_INVOKABLE qreal realValue(const QString &key);
        Q_INVOKABLE QString stringValue(const QString &key);
    };
}

#endif // SDDM_THEMECONFIG_H
