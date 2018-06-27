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

#ifndef SDDM_THEMEMETADATA_H
#define SDDM_THEMEMETADATA_H

#include <QObject>

namespace SDDM {
    class ThemeMetadataPrivate;

    class ThemeMetadata : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(ThemeMetadata)
    public:
        explicit ThemeMetadata(const QString &path, QObject *parent = 0);
        ~ThemeMetadata();

        const QString &mainScript() const;
        const QString &configFile() const;
        const QString &translationsDirectory() const;

        void setTo(const QString &path);

    private:
        ThemeMetadataPrivate *d { nullptr };
    };
}

#endif // SDDM_THEMEMETADATA_H
