/*
 * Utilities for X Display Control Protocol
 * Copyright (C) 2013  Martin Bříza <mbriza@redhat.com>
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

#include "Utils.h"

#include <QtCore/QVector>

namespace SDDM {
namespace XDMCP {

    Reader::Reader(const QByteArray &data) : m_data(data),
        m_stream(&m_data, QIODevice::ReadOnly | QIODevice::Unbuffered) {
        m_stream.setByteOrder(QDataStream::BigEndian);
    }

    Reader& Reader::operator>>(uint8_t &byte) {
        m_stream >> byte;
        return *this;
    }

    Reader& Reader::operator>>(uint16_t &word) {
        m_stream >> word;
        return *this;
    }

    Reader& Reader::operator>>(uint32_t &doubleword) {
        m_stream >> doubleword;
        return *this;
    }

    Reader& Reader::operator>>(QByteArray &array) {
        uint16_t arrayLen;
        *this >> arrayLen;
        while (arrayLen--) {
            uint8_t byte;
            *this >> byte;
            array.append(byte);
        }
        return *this;
    }

    Reader& Reader::operator>>(QVector< uint16_t > &wordArray) {
        uint8_t arrayLen;
        *this >> arrayLen;
        while (arrayLen--) {
            uint16_t word;
            *this >> word;
            wordArray.append(word);
        }
        return *this;
    }

    Reader& Reader::operator>>(QVector< QByteArray > &arrayOfArrays) {
        uint8_t arrayCount;
        *this >> arrayCount;
        while (arrayCount--) {
            QByteArray array;
            *this >> array;
            arrayOfArrays.append(array);
        }
        return *this;
    }

    bool Reader::isFinished() const {
        if ((m_stream.status() == QDataStream::Ok) && m_stream.atEnd())
            return true;
        else
            return false;
    }

    Writer::Writer() : m_data(),
        m_stream(&m_data, QIODevice::WriteOnly | QIODevice::Unbuffered) {
        m_stream.setByteOrder(QDataStream::BigEndian);
    }

    Writer& Writer::operator<<(const uint8_t byte) {
        qDebug() << "Appending:" << byte << QChar(byte);
        m_stream << byte;
        return *this;
    }

    Writer& Writer::operator<<(const uint16_t word) {
        m_stream << word;
        return *this;
    }

    Writer& Writer::operator<<(const uint32_t doubleword) {
        m_stream << doubleword;
        return *this;
    }

    Writer& Writer::operator<<(const QByteArray &array) {
        *this << (uint16_t) array.count();
        for (uint8_t c : array)
            m_stream << c;
        return *this;
    }

    Writer& Writer::operator<<(const QVector< uint16_t > &wordArray) {
        *this << (uint8_t) wordArray.count();
        for (const uint16_t &i : wordArray)
            *this << i;
        return *this;
    }

    Writer& Writer::operator<<(const QVector< QByteArray > &arrayOfArrays) {
        *this << (uint16_t) arrayOfArrays.count();
        for (const QByteArray &i : arrayOfArrays)
            *this << i;
        return *this;
    }

    QByteArray Writer::finalize(uint16_t opcode) {
        QByteArray result;
        QDataStream finalStream(&result, QIODevice::WriteOnly | QIODevice::Unbuffered);
        finalStream.setByteOrder(QDataStream::BigEndian);
        finalStream << (uint16_t) 1;
        finalStream << (uint16_t) opcode;
        finalStream << (uint16_t) m_data.size();
        for (uint8_t c : m_data)
            finalStream << c;
        return result;
    }

} // namespace XDMCP
} // namespace SDDM