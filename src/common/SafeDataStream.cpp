/*
 * QDataStream implementation for safe socket operation
 * Copyright (C) 2014  Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "SafeDataStream.h"

#include <QtCore/QDebug>
#include <QIODevice>

namespace SDDM {
    SafeDataStream::SafeDataStream(QIODevice* device)
            : QDataStream(&m_data, QIODevice::ReadWrite)
            , m_device(device) { }

    void SafeDataStream::send() {
        qint64 length = m_data.length();
        qint64 writtenTotal = 0;
        if (!m_device->isOpen()) {
            qCritical() << " Auth: SafeDataStream: Could not write any data";
            return;
        }
        m_device->write((const char*) &length, sizeof(length));
        while (writtenTotal != length) {
            qint64 written = m_device->write(m_data.mid(writtenTotal));
            if (written < 0 || !m_device->isOpen()) {
                qCritical() << " Auth: SafeDataStream: Could not write all stored data";
                return;
            }
            writtenTotal += written;
            m_device->waitForBytesWritten(-1);
        }

        reset();
    }

    void SafeDataStream::receive() {
        qint64 length = -1;

        if (!m_device->isOpen()) {
            qCritical() << " Auth: SafeDataStream: Could not read from the device";
            return;
        }
        if (!m_device->bytesAvailable())
            m_device->waitForReadyRead(-1);
        m_device->read((char*) &length, sizeof(length));

        if (length < 0)
            return;
        reset();

        while (m_data.length() < length) {
            if (!m_device->isOpen()) {
                qCritical() << " Auth: SafeDataStream: Could not read from the device";
                return;
            }
            if (!m_device->bytesAvailable())
                m_device->waitForReadyRead(-1);
            m_data.append(m_device->read(length - m_data.length()));
        }
    }

    void SafeDataStream::reset() {
        m_data.clear();
        device()->reset();
        resetStatus();
    }
}
