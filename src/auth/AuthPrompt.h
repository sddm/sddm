/*
 *
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef PROMPT_H
#define PROMPT_H

#include <QtCore/QObject>

namespace SDDM {
    class AuthRequest;
    class Prompt;
    /**
    * \brief
    * One prompt input for the authentication
    *
    * \section description
    * The main, not completely obvious rationale for this class is:
    *
    * \warning Don't use the \ref message property if you have your own strings for
    *      the \ref Type -s. PAM sends horrible horrible stuff and passwd obviously
    *      doesn't tell us a thing.
    */
    class AuthPrompt : public QObject {
        Q_OBJECT
        Q_ENUMS(Type)
        Q_PROPERTY(Type type READ type CONSTANT)
        Q_PROPERTY(QString message READ message CONSTANT)
        Q_PROPERTY(bool hidden READ hidden CONSTANT)
        Q_PROPERTY(QByteArray response READ responseFake WRITE setResponse NOTIFY responseChanged)
    public:
        virtual ~AuthPrompt();
        /**
        * \note In hex not for binary operations but to leave space for adding other codes
        */
        enum Type {
            NONE = 0x0000,           ///< No type
            UNKNOWN = 0x0001,        ///< Unknown type
            LOGIN_USER = 0x0010,     ///< On logging in: The username
            LOGIN_PASSWORD = 0x0020, ///< On logging in: The password
            CHANGE_PASSWORD = 0x0040 ///< On changing the password: The current, new or repeat password
        };
        /**
        * @return the type of the prompt
        */
        Type type() const;
        /**
        * @warning the preferred way is to use \ref type
        * @return message from the stack
        */
        QString message() const;
        /**
        * @return true if user's input should not be shown in readable form
        */
        bool hidden() const;
        /**
         * Public getter for the response data.
         * The property is write-only though, so it returns garbage.
         * Contained only to keep the MOC parser happy.
         * @warning do not use, doesn't return valid data
         * @return empty byte array
         */
        QByteArray responseFake();
        /**
        * Setter for the response data
        * @param r data entered by the user
        */
        void setResponse(const QByteArray &r);
        /**
         * @brief get string representation of AuthPrompt type, see enum @ref Type
         * @note for debug logging
         */
        static const QString &typeToString(int type);

    Q_SIGNALS:
        /**
        * Emitted when the response was entered by the user
        */
        void responseChanged();
    private:
        AuthPrompt(const Prompt *prompt, AuthRequest *parent = 0);
        QByteArray response() const;
        friend class AuthRequest;
        class Private;
        Private *d { nullptr };
    };
}

#endif //PROMPT_H
