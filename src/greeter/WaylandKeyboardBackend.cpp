/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "KeyboardLayout.h"
#include "KeyboardModel_p.h"
#include "WaylandKeyboardBackend.h"

#include <QFile>
#include <QLocale>
#include <QRegExp>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>

namespace SDDM {
    WaylandKeyboardBackend::WaylandKeyboardBackend(KeyboardModelPrivate *kmp) : KeyboardBackend(kmp) {
    }

    WaylandKeyboardBackend::~WaylandKeyboardBackend() {
    }

    void WaylandKeyboardBackend::init() {
        // regexp to match country codes
        QRegExp re(R"([a-z]+)\-([a-z]+)");
        re.setCaseSensitivity(Qt::CaseInsensitive);

        // possible layouts based on system locale
        QSet<QString> langs;
        QStringList uiLangs = QLocale::system().uiLanguages();
        for (QString lang: uiLangs) {
            // Defaults to us
            if (lang == QStringLiteral("C")) {
                langs.insert(QStringLiteral("us"));
                continue;
            }

            // extract two letters country code
            if (re.indexIn(lang) != -1)
                langs.insert(re.cap(2));
        }

        // return only layouts matching system locale
        const QString fileName = QStandardPaths::locate(
                    QStandardPaths::GenericDataLocation,
                    "X11/xkb/rules/base.lst");
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QRegExp layoutRe("\\s*([a-z]{2})\\s+(\\w+)");
        bool found = false;

        while (!file.atEnd()) {
            QByteArray line = file.readLine();

            if (line.startsWith("!")) {
                found = line.startsWith("! layout");
                continue;
            }

            if (!found)
                continue;
            if (layoutRe.indexIn(QString(line)) == -1)
                continue;

            if (langs.contains(layoutRe.cap(1)))
                d->layouts << new KeyboardLayout(layoutRe.cap(1), layoutRe.cap(2));
        }

        file.close();
    }

    void WaylandKeyboardBackend::disconnect() {
    }

    void WaylandKeyboardBackend::sendChanges() {
    }

    void WaylandKeyboardBackend::dispatchEvents() {
    }

    void WaylandKeyboardBackend::connectEventsDispatcher(KeyboardModel *model) {
    }
}
