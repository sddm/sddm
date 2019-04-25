/***************************************************************************
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

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#ifndef EMBEDDED
#include "XcbKeyboardBackend.h"
#else
#include "EmbeddedKeyboardBackend.h"
#endif
#include "KeyboardLayout.h"

namespace SDDM {
    /**********************************************/
    /* KeyboardModel                              */
    /**********************************************/

    KeyboardModel::KeyboardModel() : d(new KeyboardModelPrivate) {
#ifndef EMBEDDED
        m_backend = new XcbKeyboardBackend(d);
        m_backend->init();
        m_backend->connectEventsDispatcher(this);
#else
        m_backend = new EmbeddedKeyboardBackend(d);
        m_backend->init();
#endif
    }

    KeyboardModel::~KeyboardModel() {
#ifndef EMBEDDED
        m_backend->disconnect();
#endif
        delete m_backend;

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
#ifndef EMBEDDED
            m_backend->sendChanges();
#endif

            emit numLockStateChanged();
        }
    }

    bool KeyboardModel::capsLockState() const {
        return d->capslock.enabled;
    }

    void KeyboardModel::setCapsLockState(bool state) {
        if (d->capslock.enabled != state) {
            d->capslock.enabled = state;
#ifndef EMBEDDED
            m_backend->sendChanges();
#endif

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
            m_backend->sendChanges();

            emit currentLayoutChanged();
        }
    }

    bool KeyboardModel::enabled() const {
        return d->enabled;
    }

#ifdef EMBEDDED
    void KeyboardModel::setLayouts(const QStringList &layouts) {
        if (layouts.isEmpty())
            return;

        d->layouts.clear();
        for(int i = 0; i < layouts.size(); ++i) {
            QString nshort = layouts[i].left(layouts[i].indexOf(QLatin1Char('_')));
            d->layouts << new KeyboardLayout(nshort, layouts[i]);
        }
        emit layoutsChanged();
    }
#endif

    void KeyboardModel::dispatchEvents() {
        // Save old states
        bool num_old = d->numlock.enabled, caps_old = d->capslock.enabled;
        int layout_old = d->layout_id;
        QList<QObject*> layouts_old = d->layouts;

        // Process events
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
