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
#include <string.h>
#include <QElapsedTimer>

namespace SDDM {
    SafeDataStream::SafeDataStream(QIODevice* device)
            : QDataStream(&m_data, QIODevice::ReadWrite)
            , m_device(device) { }

    SafeDataStream::~SafeDataStream() {
        explicit_bzero(m_data.data(), m_data.size());
    }

    void SafeDataStream::send() {
        const qint64 length = m_data.length();
        if (!m_device->isOpen()) { setStatus(QDataStream::WriteFailed); return; }
        QByteArray frame(reinterpret_cast<const char *>(&length), sizeof(length));
        frame.append(m_data);
        qint64 sent = 0;
        QElapsedTimer deadline;
        deadline.start();
        while (sent < frame.size()) {
            const qint64 count = m_device->write(frame.constData() + sent, frame.size() - sent);
            if (count < 0 || deadline.elapsed() >= 1000) {
                explicit_bzero(frame.data(), frame.size());
                setStatus(QDataStream::WriteFailed);
                return;
            }
            sent += count;
            if (m_device->bytesToWrite() && !m_device->waitForBytesWritten(1000 - deadline.elapsed())) {
                explicit_bzero(frame.data(), frame.size());
                setStatus(QDataStream::WriteFailed);
                return;
            }
        }
        explicit_bzero(frame.data(), frame.size());
        reset();
    }

    void SafeDataStream::receive(bool waitForReply) {
        reset();
        if (waitForReply && !m_device->bytesAvailable() && !m_device->waitForReadyRead(-1)) {
            setStatus(QDataStream::ReadPastEnd);
            return;
        }
        qint64 length = -1;
        QElapsedTimer deadline;
        deadline.start();
        auto readExact = [this, &deadline](char *destination, qint64 size) {
            qint64 received = 0;
            while (received < size) {
                if (!m_device->isOpen() || deadline.elapsed() >= 1000) return false;
                const qint64 count = m_device->read(destination + received, size - received);
                if (count < 0) return false;
                received += count;
                if (received < size && !m_device->bytesAvailable()
                    && !m_device->waitForReadyRead(1000 - deadline.elapsed())) return false;
            }
            return true;
        };
        if (!readExact(reinterpret_cast<char *>(&length), sizeof(length))) {
            setStatus(QDataStream::ReadPastEnd);
            return;
        }
        // PAM messages and session environments must fit in a bounded frame.
        if (length < 0 || length > 1024 * 1024) { setStatus(QDataStream::ReadCorruptData); return; }
        m_data.resize(length);
        if (!readExact(m_data.data(), length)) {
            explicit_bzero(m_data.data(), m_data.size());
            m_data.clear();
            setStatus(QDataStream::ReadPastEnd);
        }
    }

    void SafeDataStream::reset() {
        explicit_bzero(m_data.data(), m_data.size());
        m_data.clear();
        device()->reset();
        resetStatus();
    }
}
