/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* SPDX-FileCopyrightText: 2010 Andriy Rysin <rysin@kde.org>
* SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>
* SPDX-FileCopyrightText: 2022 Volker Krause <vkrause>
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

#include <QDir>
#include <QDebug>
#include <QGuiApplication>
#include <QInputMethod>

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#include "KeyboardLayout.h"
#include "waylandkeyboardbackend.h"
#include <qxmlstream.h>

namespace SDDM {

WaylandKeyboardBackend::WaylandKeyboardBackend(KeyboardModelPrivate *kmp)
    : KeyboardBackend(kmp)
{
}

WaylandKeyboardBackend::~WaylandKeyboardBackend()
{
}


QList<QObject *> parseRules(const QString &filename, int &current)
{
    // FIXME: https://github.com/sddm/sddm/pull/1664#discussion_r1115361314
    current = 0;
    QFile file(filename);
    qDebug() << "Parsing xkb rules from" << file.fileName();
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "Cannot open the rules file" << file.fileName();
        return {};
    }

    QList<QObject *> layouts;

    QString lastName, lastDescription;

    QStringList path;
    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            path << reader.name().toString();
            QString strPath = path.join(QLatin1String("/"));

            if (strPath.endsWith(QLatin1String("layoutList/layout/configItem/name"))) {
                lastName = reader.readElementText().trimmed();
            } else if (strPath.endsWith(QLatin1String("layoutList/layout/configItem/description"))) {
                // TODO: This should be translated using i18nd("xkeyboard-config", lastDescription)
                lastDescription = reader.readElementText().trimmed();
            }
        }
        // don't use token here, readElementText() above can have moved us forward meanwhile
        if (reader.tokenType() == QXmlStreamReader::EndElement) {
            const QString strPath = path.join(QLatin1String("/"));
            if (strPath.endsWith(QLatin1String("layoutList/layout/configItem/description"))) {
                layouts << new KeyboardLayout(lastName, lastDescription);
            }
            path.removeLast();
        }
    }

    if (reader.hasError()) {
        qWarning() << "Failed to parse the rules file" << file.fileName();
        return {};
    }
    return layouts;
}

void WaylandKeyboardBackend::init()
{
    d->layouts = parseRules(QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml"), d->layout_id);
}

void WaylandKeyboardBackend::disconnect()
{
}

void WaylandKeyboardBackend::sendChanges()
{
}

void WaylandKeyboardBackend::dispatchEvents()
{
}

void WaylandKeyboardBackend::connectEventsDispatcher(KeyboardModel *model)
{
    Q_UNUSED(model);
}

} // namespace SDDM
