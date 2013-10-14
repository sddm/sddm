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

#ifndef SDDM_XDMCP_UTILS_H
#define SDDM_XDMCP_UTILS_H

#include <QtCore/QByteArray>
#include <QtCore/QDataStream>

#include "Packet.h"

namespace SDDM {
namespace XDMCP {

    /**
     * Class for reading information from raw packets and setting the right byte order
     *
     * Workflow is as follows:
     * * Construct Reader from the data received
     * * Using the stream operator extract all required variables
     * * Check if the stream is at its end by isFinished()
     */
    class Reader {
        public:
            Reader(const QByteArray &data);
            ~Reader() {}
            Reader& operator>>(uint8_t& byte);
            Reader& operator>>(uint16_t& word);
            Reader& operator>>(uint32_t& doubleword);
            Reader& operator>>(QByteArray& array);
            Reader& operator>>(QVector<uint16_t>& wordArray);
            Reader& operator>>(QVector<QByteArray>& arrayOfArrays);
            /**
             * Returns true if the stream is at its end and no errors occured
             *
             * \return Finished status
             */
            bool isFinished() const;
        private:
            QByteArray m_data;
            QDataStream m_stream;
    };

    /**
     * Class for writing information to raw packets and setting the right byte order
     *
     * Workflow is as follows:
     * * Construct empty writer
     * * Using the stream operator insert all contained variables
     * * Get a complete packet by the finalize(opcode) method
     */
    class Writer {
        public:
            Writer();
            Writer& operator<<(const uint8_t byte);
            Writer& operator<<(const uint16_t word);
            Writer& operator<<(const uint32_t doubleword);
            Writer& operator<<(const QByteArray& array);
            Writer& operator<<(const QVector<uint16_t>& wordArray);
            Writer& operator<<(const QVector<QByteArray>& arrayOfArrays);
            /**
             * Finalizes building of the packet
             *
             * \param opcode XDMCP protocol code of the packet type
             * \return Raw packet data
             */
            QByteArray finalize(uint16_t opcode);
        private:
            QByteArray m_data;
            QDataStream m_stream;
    };

} // namespace XDMCP
} // namespace SDDM

#endif // SDDM_XDMCP_UTILS_H
