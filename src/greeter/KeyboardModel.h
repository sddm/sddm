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

#ifndef KEYBOARDMODEL_H
#define KEYBOARDMODEL_H

#include <QList>
#include <QObject>
#include <QString>

namespace SDDM {
    class KeyboardModelPrivate;
    class KeyboardBackend;

    class KeyboardModel : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(KeyboardModel)
    public:
        // LED control
        Q_PROPERTY(bool numLock  READ numLockState  WRITE setNumLockState  NOTIFY numLockStateChanged)
        Q_PROPERTY(bool capsLock READ capsLockState WRITE setCapsLockState NOTIFY capsLockStateChanged)

        // Layouts control
        Q_PROPERTY(int currentLayout READ currentLayout WRITE setCurrentLayout NOTIFY currentLayoutChanged)
        Q_PROPERTY(QList<QObject*> layouts READ layouts NOTIFY layoutsChanged)

        Q_PROPERTY(bool enabled READ enabled CONSTANT)

    public:
        KeyboardModel();
        virtual ~KeyboardModel();

    signals:
        void numLockStateChanged();
        void capsLockStateChanged();

        void currentLayoutChanged();
        void layoutsChanged();

    public slots:
        bool numLockState() const;
        void setNumLockState(bool state);

        bool capsLockState() const;
        void setCapsLockState(bool state);

        QList<QObject*> layouts() const;
        int currentLayout() const;
        void setCurrentLayout(int id);

        bool enabled() const;

    private slots:
        void dispatchEvents();

    private:
        KeyboardModelPrivate * d { nullptr };
        KeyboardBackend * m_backend = nullptr;
    };
}

#endif // KEYBOARDMODEL_H
