/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#include "Display.h"

#include "Authenticator.h"
#include "Configuration.h"
#include "Constants.h"
#include "DisplayServer.h"
#include "SocketServer.h"
#include "Greeter.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTimer>

namespace SDDM {
    QString generateName(int length) {
        QString digits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        // reserve space for name
        QString name;
        name.reserve(length);

        // create random device
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, digits.length() - 1);

        // generate name
        for (int i = 0; i < length; ++i)
            name[i] = digits.at(dis(gen));

        // return result
        return name;
    }

    Display::Display(const QString &display, QObject *parent) : QObject(parent),
        m_display(display),
        m_authenticator(new Authenticator(this)),
        m_displayServer(new DisplayServer(this)),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this)) {
        // connect signals
        connect(m_socketServer, SIGNAL(login(QLocalSocket*,QString,QString,QString)), this, SLOT(login(QLocalSocket*,QString,QString,QString)));

        connect(this, SIGNAL(loginFailed(QLocalSocket*)), m_socketServer, SLOT(loginFailed(QLocalSocket*)));
        connect(this, SIGNAL(loginSucceeded(QLocalSocket*)), m_socketServer, SLOT(loginSucceeded(QLocalSocket*)));

#if !TEST
        // create auth dir if not existing
        QDir authDir;
        authDir.mkpath(Configuration::instance()->authDir());

        // set auth path
        m_authPath = QString("%1/A%2-%3").arg(Configuration::instance()->authDir()).arg(m_display).arg(generateName(6));
#else
        // set auth path
        m_authPath = "./sddm.auth";
#endif

        // set socket name
        m_socket = QString("sddm-%1-%2").arg(m_display).arg(generateName(6));
    }

    Display::~Display() {
        stop();
    }

    const QString &Display::name() const {
        return m_display;
    }

    const QString &Display::cookie() const {
        return m_cookie;
    }

    const QString &Display::seatId() const {
        return m_seatId;
    }

    const int Display::vtNumber() const {
        return m_vtNumber;
    }

    void Display::start() {
        // check flag
        if (m_started)
            return;

        // set authenticator params
        m_authenticator->setDisplay(m_display);

        // generate cookie
        m_cookie = m_authenticator->generateCookie();

        // generate auth file
        m_authenticator->addCookie(m_authPath);

        // set display server params
        m_displayServer->setDisplay(m_display);
        m_displayServer->setAuthPath(m_authPath);

        // start display server
        m_displayServer->start();

        if (m_first && !Configuration::instance()->autoUser().isEmpty() && !Configuration::instance()->lastSession().isEmpty()) {
            // set flags
            m_first = false;
            m_started = true;

            // start session
            m_authenticator->start(Configuration::instance()->autoUser(), Configuration::instance()->lastSession());

            // wait until session ends
            m_authenticator->waitForFinished();

            // stop
            stop();

            // restart display
            QTimer::singleShot(1, this, SLOT(start()));

            // return
            return;
        }

        // set socket server name
        m_socketServer->setSocket(m_socket);

        // start socket server
        m_socketServer->start();

        // set greeter params
        m_greeter->setDisplay(m_display);
        m_greeter->setAuthPath(m_authPath);
        m_greeter->setSocket(m_socket);
        m_greeter->setTheme(QString("%1/%2").arg(Configuration::instance()->themesDir()).arg(Configuration::instance()->currentTheme()));

        // start greeter
        m_greeter->start();

        // set flags
        m_first = false;
        m_started = true;
    }

    void Display::stop() {
        // check flag
        if (!m_started)
            return;

        // stop user session
        m_authenticator->stop();

        // stop the greeter
        m_greeter->stop();

        // stop socket server
        m_socketServer->stop();

        // stop display server
        m_displayServer->stop();

        // remove authority file
        QFile::remove(m_authPath);

        // reset flag
        m_started = false;
    }

    void Display::login(QLocalSocket *socket, const QString &user, const QString &password, const QString &session) {
        // authenticate
        if (!m_authenticator->authenticate(user, password)) {
            // emit signal
            emit loginFailed(socket);

            // return
            return;
        }

        // start session
        if (!m_authenticator->start(user, session)) {
            // emit signal
            emit loginFailed(socket);

            // return
            return;
        }

        // save last user and last session
        Configuration::instance()->setLastUser(user);
        Configuration::instance()->setLastSession(session);
        Configuration::instance()->save();

        // emit signal
        emit loginSucceeded(socket);

        // wait until session ends
        m_authenticator->waitForFinished();

        // stop
        stop();

        // restart display
        QTimer::singleShot(1, this, SLOT(start()));
    }
}
