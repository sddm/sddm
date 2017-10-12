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

#include "ThemeMetadata.h"

#include <QSettings>

namespace SDDM {
    class ThemeMetadataPrivate {
    public:
        QString mainScript { QStringLiteral("Main.qml") };
        QString configFile;
        QString translationsDirectory { QStringLiteral(".") };
    };

    ThemeMetadata::ThemeMetadata(const QString &path, QObject *parent) : QObject(parent), d(new ThemeMetadataPrivate()) {
       setTo(path);
    }

    ThemeMetadata::~ThemeMetadata() {
        delete d;
    }

    const QString &ThemeMetadata::mainScript() const {
        return d->mainScript;
    }

    const QString &ThemeMetadata::configFile() const {
        return d->configFile;
    }

    const QString &ThemeMetadata::translationsDirectory() const {
        return d->translationsDirectory;
    }

    void ThemeMetadata::setTo(const QString &path) {
        QSettings settings(path, QSettings::IniFormat);
        // read values
        d->mainScript = settings.value(QStringLiteral("SddmGreeterTheme/MainScript"), QStringLiteral("Main.qml")).toString();
        d->configFile = settings.value(QStringLiteral("SddmGreeterTheme/ConfigFile"), QStringLiteral("theme.conf")).toString();
        d->translationsDirectory = settings.value(QStringLiteral("SddmGreeterTheme/TranslationsDirectory"), QStringLiteral(".")).toString();
    }
}
