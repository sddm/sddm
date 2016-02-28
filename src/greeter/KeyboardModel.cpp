/***************************************************************************
* Copyright (c) 2021 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Nikita Mikhaylov <nslqqq@gmail.com>
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

#include <QGuiApplication>

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#include "waylandkeyboardbackend.h"
#include "XcbKeyboardBackend.h"

namespace SDDM {
    /**********************************************/
    /* KeyboardModel                              */
    /**********************************************/

    KeyboardModel::KeyboardModel() : d(new KeyboardModelPrivate) {
        if (QGuiApplication::platformName() == QLatin1String("xcb")) {
            m_backend = new XcbKeyboardBackend(d);
            m_backend->init();
            m_backend->connectEventsDispatcher(this);
        } else if (QGuiApplication::platformName().contains(QLatin1String("wayland"))) {
            m_backend = new WaylandKeyboardBackend(d);
            m_backend->init();
        }
    }

    KeyboardModel::~KeyboardModel() {
        if (m_backend) {
            m_backend->disconnect();
            delete m_backend;
        }

        for (QObject *layout: d->layouts) {
            delete layout;
        }
        delete d;
    }

    bool KeyboardModel::numLockState() const {
        return d->numlock.enabled;
    }

    void KeyboardModel::setNumLockState(bool state) {
        if (d->numlock.enabled != state) {
            d->numlock.enabled = state;
            if (m_backend)
                m_backend->sendChanges();

            emit numLockStateChanged();
        }
    }

    bool KeyboardModel::capsLockState() const {
        return d->capslock.enabled;
    }

    void KeyboardModel::setCapsLockState(bool state) {
        if (d->capslock.enabled != state) {
            d->capslock.enabled = state;
            if (m_backend)
                m_backend->sendChanges();

            emit capsLockStateChanged();
        }
    }

    QList<QObject*> KeyboardModel::layouts() const {
        return d->layouts;
    }

    int KeyboardModel::currentLayout() const {
        return d->layout_id;
    }

    void KeyboardModel::setCurrentLayout(int id) {
        if (d->layout_id != id) {
            d->layout_id = id;
            if (m_backend)
                m_backend->sendChanges();

            emit currentLayoutChanged();
        }
    }

    bool KeyboardModel::enabled() const {
        return d->enabled;
    }

    void KeyboardModel::dispatchEvents() {
        // Save old states
        bool num_old = d->numlock.enabled, caps_old = d->capslock.enabled;
        int layout_old = d->layout_id;
        QList<QObject*> layouts_old = d->layouts;

        // Process events
        if (m_backend)
            m_backend->dispatchEvents();

        // Send updates
        if (caps_old != d->capslock.enabled)
            emit capsLockStateChanged();

        if (num_old != d->numlock.enabled)
            emit numLockStateChanged();

        if (layout_old != d->layout_id)
            emit currentLayoutChanged();

        if (layouts_old != d->layouts)
            emit layoutsChanged();
    }
}

#include "moc_KeyboardModel.cpp"
