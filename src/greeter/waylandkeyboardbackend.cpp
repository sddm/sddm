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

#include "waylandkeyboardbackend.h"

#include "KeyboardLayout.h"
#include "KeyboardModel.h"
#include "KeyboardModel_p.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

#include <algorithm>

namespace SDDM {

static const QString locale1Service = QStringLiteral("org.freedesktop.locale1");
static const QString locale1Path = QStringLiteral("/org/freedesktop/locale1");
static const QString locale1Iface = QStringLiteral("org.freedesktop.locale1");
static const QString dbusPropertiesIface = QStringLiteral("org.freedesktop.DBus.Properties");

WaylandKeyboardBackend::WaylandKeyboardBackend(KeyboardModelPrivate *kmp)
    : KeyboardBackend(kmp)
{
}

WaylandKeyboardBackend::~WaylandKeyboardBackend()
{
}

void WaylandKeyboardBackend::applyLocale1(const Locale1Keyboard &kb)
{
    qDeleteAll(d->layouts);
    d->layouts.clear();
    d->layoutIds = kb.layouts;
    d->variantIds = kb.variants;
    d->x11Model = kb.model;
    d->x11Options = kb.options;
    d->layout_id = 0;

    for (const QString &id : kb.layouts)
        d->layouts << new KeyboardLayout(id, id);

    d->enabled = !d->layouts.isEmpty();
}

bool WaylandKeyboardBackend::readLocale1()
{
    QDBusInterface iface(locale1Service, locale1Path, dbusPropertiesIface, QDBusConnection::systemBus());
    if (!iface.isValid()) {
        qWarning() << "Cannot talk to locale1:" << iface.lastError().message();
        return false;
    }

    const QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetAll"), locale1Iface);
    if (!reply.isValid()) {
        qWarning() << "locale1 GetAll failed:" << reply.error().message();
        return false;
    }

    applyLocale1(parseLocale1Keyboard(reply.value()));
    return d->enabled;
}

void WaylandKeyboardBackend::init()
{
    if (!readLocale1()) {
        qWarning() << "Wayland keyboard backend: no layouts from locale1";
        d->enabled = false;
    }
}

void WaylandKeyboardBackend::disconnect()
{
    QDBusConnection::systemBus().disconnect(
        locale1Service, locale1Path, dbusPropertiesIface, QStringLiteral("PropertiesChanged"),
        this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
}

void WaylandKeyboardBackend::sendChanges()
{
    if (!m_model || d->layoutIds.isEmpty())
        return;
    if (d->layout_id < 0 || d->layout_id >= d->layoutIds.size())
        return;

    QStringList layouts = d->layoutIds;
    QStringList variants = d->variantIds;
    while (variants.size() < layouts.size())
        variants << QString();
    variants.resize(layouts.size());

    const int id = d->layout_id;
    std::rotate(layouts.begin(), layouts.begin() + id, layouts.end());
    std::rotate(variants.begin(), variants.begin() + id, variants.end());

    m_model->requestLayoutChange(layouts.join(QLatin1Char(',')),
                                 d->x11Model,
                                 variants.join(QLatin1Char(',')),
                                 d->x11Options);
}

void WaylandKeyboardBackend::dispatchEvents()
{
    readLocale1();
}

void WaylandKeyboardBackend::connectEventsDispatcher(KeyboardModel *model)
{
    m_model = model;
    QDBusConnection::systemBus().connect(
        locale1Service, locale1Path, dbusPropertiesIface, QStringLiteral("PropertiesChanged"),
        this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
}

void WaylandKeyboardBackend::propertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated);
    if (interface != locale1Iface)
        return;
    if (!changed.contains(QStringLiteral("X11Layout"))
        && !changed.contains(QStringLiteral("X11Variant"))
        && !changed.contains(QStringLiteral("X11Model"))
        && !changed.contains(QStringLiteral("X11Options")))
        return;
    if (m_model)
        m_model->dispatchEvents();
}

} // namespace SDDM
