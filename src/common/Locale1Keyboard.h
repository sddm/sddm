/***************************************************************************
* Copyright (c) 2026 Simon Quigley
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

#ifndef SDDM_LOCALE1KEYBOARD_H
#define SDDM_LOCALE1KEYBOARD_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace SDDM {

struct Locale1Keyboard {
    QStringList layouts;
    QStringList variants;
    QString model;
    QString options;
};

inline QStringList splitLocale1List(const QString &value)
{
    if (value.trimmed().isEmpty())
        return {};
    return value.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

inline Locale1Keyboard parseLocale1Keyboard(const QVariantMap &properties)
{
    Locale1Keyboard kb;
    kb.layouts = splitLocale1List(properties.value(QStringLiteral("X11Layout")).toString());
    kb.variants = splitLocale1List(properties.value(QStringLiteral("X11Variant")).toString());
    kb.model = properties.value(QStringLiteral("X11Model")).toString();
    kb.options = properties.value(QStringLiteral("X11Options")).toString();
    return kb;
}

}

#endif
