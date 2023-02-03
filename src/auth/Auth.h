/*
 * Qt Authentication library
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

#ifndef SDDM_AUTH_H
#define SDDM_AUTH_H

#include "AuthRequest.h"
#include "AuthPrompt.h"

#include <QtCore/QObject>
#include <QtCore/QProcessEnvironment>

namespace SDDM {
    /**
    * \brief
    * Main class triggering the authentication and handling all communication
    *
    * \section description
    * There are three basic kinds of authentication:
    *
    *  * Checking only the validity of the user's secrets - The default values
    *
    *  * Logging the user in after authenticating him - You'll have to set the
    *      \ref session property to do that.
    *
    *  * Logging the user in without authenticating - You'll have to set the
    *      \ref session and \ref autologin properties to do that.
    *
    * Usage:
    *
    * Just construct, connect the signals (especially \ref requestChanged)
    * and fire up \ref start
    */
    class Auth : public QObject {
        Q_OBJECT
        // not setting NOTIFY for the properties - they should be set only once before calling start
        Q_PROPERTY(bool autologin READ autologin WRITE setAutologin NOTIFY autologinChanged)
        Q_PROPERTY(bool greeter READ isGreeter WRITE setGreeter NOTIFY greeterChanged)
        Q_PROPERTY(bool verbose READ verbose WRITE setVerbose NOTIFY verboseChanged)
        Q_PROPERTY(QByteArray cookie READ cookie WRITE setCookie NOTIFY cookieChanged)
        Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
        Q_PROPERTY(QString session READ session WRITE setSession NOTIFY sessionChanged)
        Q_PROPERTY(AuthRequest* request READ request NOTIFY requestChanged)
    public:
        explicit Auth(const QString &user = QString(), const QString &session = QString(), bool autologin = false, QObject *parent = 0, bool verbose = false);
        explicit Auth(QObject *parent);
        ~Auth();

        enum Info {
            INFO_NONE = 0,
            INFO_UNKNOWN,
            INFO_PASS_CHANGE_REQUIRED,
            _INFO_LAST
        };
        Q_ENUM(Info)

        enum Error {
            ERROR_NONE = 0,
            ERROR_UNKNOWN,
            ERROR_AUTHENTICATION,
            ERROR_INTERNAL,
            _ERROR_LAST
        };
        Q_ENUM(Error)

        enum HelperExitStatus {
            HELPER_SUCCESS = 0,
            HELPER_AUTH_ERROR,
            HELPER_SESSION_ERROR,
            HELPER_OTHER_ERROR,
            HELPER_DISPLAYSERVER_ERROR,
            HELPER_TTY_ERROR,
        };
        Q_ENUM(HelperExitStatus)

        static void registerTypes();

        bool autologin() const;
        bool isGreeter() const;
        bool verbose() const;
        const QByteArray &cookie() const;
        const QString &user() const;
        const QString &session() const;
        AuthRequest *request();
        /**
         * True if an authentication or session is in progress
         */
        bool isActive() const;

        /**
        * If starting a session, you will probably want to provide some basic env variables for the session.
        * This only inserts the variables - if the current key already had a value, it will be overwritten.
        * User-specific data such as $HOME is generated automatically.
        * @param env the environment
        */
        void insertEnvironment(const QProcessEnvironment &env);

        /**
        * Works the same as \ref insertEnvironment but only for one key-value pair
        * @param key key
        * @param value value
        */
        void insertEnvironment(const QString &key, const QString &value);

        /**
        * Set mode to autologin.
        * Ignored if session is not started
        * @param on true if should autologin
        */
        void setAutologin(bool on = true);

        /**
         * Set mode to greeter
         * This will bypass authentication checks
         */
        void setGreeter(bool on = true);

        /**
        * Forwards the output of the underlying authenticator to the current process
        * @param on true if should forward the output
        */
        void setVerbose(bool on = true);

        /**
        * Sets the user which will then authenticate
        * @param user username
        */
        void setUser(const QString &user);

        /**
         * Set the display server command to be started before the greeter.
         * @param command Command of the display server to be started
         */
        void setDisplayServerCommand(const QString &command);

        /**
        * Set the session to be started after authenticating.
        * @param path Path of the session executable to be started
        */
        void setSession(const QString &path);

        /**
         * Set the display server cookie, to be inserted into the user's $XAUTHORITY
         * @param cookie cookie data
         */
        void setCookie(const QByteArray &cookie);

    public Q_SLOTS:
        /**
        * Sets up the environment and starts the authentication
        */
        void start();

        /**
         * Indicates that we do not need the process anymore.
         */
        void stop();

    Q_SIGNALS:
        void autologinChanged();
        void greeterChanged();
        void verboseChanged();
        void cookieChanged();
        void userChanged();
        void displayServerCommandChanged();
        void sessionChanged();
        void requestChanged();

        /**
        * Emitted when authentication phase finishes
        *
        * @note If you want to set some environment variables for the session right before the
        * session is started, connect to this signal using a blocking connection and insert anything
        * you need in the slot.
        * @param user username
        * @param success true if succeeded
        */
        void authentication(QString user, bool success);

        /**
        * Emitted when session starting phase finishes
        *
        * @param success true if succeeded
        */
        void sessionStarted(bool success);

        /**
         * Emitted when the display server is ready.
         *
         * @param displayName display name
         */
        void displayServerReady(const QString &displayName);

        /**
        * Emitted when the helper quits, either after authentication or when the session ends.
        * Or, when something goes wrong.
        *
        * @param success true if every underlying task went fine
        */
        void finished(Auth::HelperExitStatus status);

        /**
        * Emitted on error
        *
        * @param message message to be displayed to the user
        */
        void error(QString message, Auth::Error type);

        /**
        * Information from the underlying stack is to be presented to the user
        *
        * @param message message to be displayed to the user
        */
        void info(QString message, Auth::Info type);

    private:
        class Private;
        class SocketServer;
        friend Private;
        friend SocketServer;
        Private *d { nullptr };
    };
}

#endif // SDDM_AUTH_H
