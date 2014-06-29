/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2014 Martin Bříza <mbriza@redhat.com>
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

#include "Configuration.h"
#include "DaemonApp.h"
#include "DisplayManager.h"
#ifdef USE_WAYLAND
#  include "WaylandDisplayServer.h"
#else
#  include "XorgDisplayServer.h"
#endif
#include "Seat.h"
#include "SocketServer.h"
#include "Greeter.h"
#include "Utils.h"
#include "SignalHandler.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTimer>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Display::Display(const int displayId, const int terminalId, Seat *parent) : QObject(parent),
        m_displayId(displayId), m_terminalId(terminalId),
        m_auth(new Auth(this)),
#ifdef USE_WAYLAND
        m_displayServer(new WaylandDisplayServer(this)),
#else
        m_displayServer(new XorgDisplayServer(this)),
#endif
        m_seat(parent),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this)) {

        // respond to authentication requests
        m_auth->setVerbose(true);
        connect(m_auth, SIGNAL(requestChanged()), this, SLOT(slotRequestChanged()));
        connect(m_auth, SIGNAL(authentication(QString,bool)), this, SLOT(slotAuthenticationFinished(QString,bool)));
        connect(m_auth, SIGNAL(session(bool)), this, SLOT(slotSessionStarted(bool)));
        connect(m_auth, SIGNAL(finished(bool)), this, SLOT(slotHelperFinished(bool)));
        connect(m_auth, SIGNAL(info(QString,Auth::Info)), this, SLOT(slotAuthInfo(QString,Auth::Info)));
        connect(m_auth, SIGNAL(error(QString,Auth::Error)), this, SLOT(slotAuthError(QString,Auth::Error)));

        // restart display after display server ended
        connect(m_displayServer, SIGNAL(stopped()), this, SLOT(stop()));

        // notify the display after display server started
        connect(m_displayServer, SIGNAL(started()), this, SLOT(displayServerStarted()));

        // connect login signal
        connect(m_socketServer, SIGNAL(login(QLocalSocket*,QString,QString,QString)), this, SLOT(login(QLocalSocket*,QString,QString,QString)));

        // connect login result signals
        connect(this, SIGNAL(loginFailed(QLocalSocket*)), m_socketServer, SLOT(loginFailed(QLocalSocket*)));
        connect(this, SIGNAL(loginSucceeded(QLocalSocket*)), m_socketServer, SLOT(loginSucceeded(QLocalSocket*)));
    }

    Display::~Display() {
        stop();
    }

    const int Display::displayId() const {
        return m_displayId;
    }

    const int Display::terminalId() const {
        return m_terminalId;
    }

    const QString &Display::name() const {
        return m_displayServer->display();
    }

    QString Display::sessionType() const {
        return m_displayServer->sessionType();
    }

    Seat *Display::seat() const {
        return m_seat;
    }

    void Display::start() {
        // check flag
        if (m_started)
            return;

        // get runtime directory and create it
        QString runtimeDir = daemonApp->configuration()->runtimeDir();
        QDir().rmpath(runtimeDir);
        QDir().mkpath(runtimeDir);

        // change owner and group
        if (!daemonApp->configuration()->testing)
            changeOwner(runtimeDir);

        // start display server
        m_displayServer->start();
    }

    void Display::displayServerStarted() {
        // check flag
        if (m_started)
            return;

        // setup display
        m_displayServer->setupDisplay();

        // log message
        qDebug() << "Display server started.";

        if ((daemonApp->configuration()->first || daemonApp->configuration()->autoRelogin()) &&
            !daemonApp->configuration()->autoUser().isEmpty() && !daemonApp->configuration()->lastSession().isEmpty()) {
            // reset first flag
            daemonApp->configuration()->first = false;

            // set flags
            m_started = true;

            // start session
            m_auth->setAutologin(true);
            startAuth(daemonApp->configuration()->autoUser(), QString(), daemonApp->configuration()->lastSession());

            // return
            return;
        }

        // start socket server
        m_socketServer->start(m_displayServer->display());

        if (!daemonApp->configuration()->testing) {
            // change the owner and group of the socket to avoid permission denied errors
            struct passwd *pw = getpwnam("sddm");
            if (pw) {
                if (chown(qPrintable(m_socketServer->socketAddress()), pw->pw_uid, pw->pw_gid) == -1) {
                    qWarning() << "Failed to change owner of the socket";
                    return;
                }
            }
        }

        // set greeter params
        m_greeter->setDisplay(this);
#ifndef USE_WAYLAND
        m_greeter->setAuthPath(qobject_cast<XorgDisplayServer *>(m_displayServer)->authPath());
#endif
        m_greeter->setSocket(m_socketServer->socketAddress());
        m_greeter->setTheme(QString("%1/%2").arg(daemonApp->configuration()->themesDir()).arg(daemonApp->configuration()->currentTheme()));

        // start greeter
        m_greeter->start();

        // reset first flag
        daemonApp->configuration()->first = false;

        // set flags
        m_started = true;
    }

    void Display::stop() {
        // check flag
        if (!m_started)
            return;

        // stop the greeter
        m_greeter->stop();

        // stop socket server
        m_socketServer->stop();

        // stop display server
        m_displayServer->blockSignals(true);
        m_displayServer->stop();
        m_displayServer->blockSignals(false);

        // reset flag
        m_started = false;

        // remove runtime directory
        QDir().rmpath(daemonApp->configuration()->runtimeDir());

        // emit signal
        emit stopped();
    }

    void Display::login(QLocalSocket *socket, const QString &user, const QString &password, const QString &session) {
        m_socket = socket;
        startAuth(user, password, session);
    }

    void Display::startAuth(const QString &user, const QString &password, const QString &session) {
        QString sessionName;
        QString xdgSessionName;
        QString command;

        m_passPhrase = password;

        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(daemonApp->configuration()->sessionsDir());

            // session file
            QFile file(dir.absoluteFilePath(session));

            // open file
            if (file.open(QIODevice::ReadOnly)) {
                // read line-by-line
                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine();

                    // line starting with Exec
                    if (line.startsWith("Exec="))
                        command = line.mid(5);

                    // Desktop names, change the separator
                    if (line.startsWith("DesktopNames=")) {
                        xdgSessionName = line.mid(13);
                        xdgSessionName.replace(';', ':');
                    }
                }

                // close file
                file.close();
            }

            // remove extension
            sessionName = session.left(session.lastIndexOf("."));
        } else {
            command = session;
            sessionName = session;
        }

        if (command.isEmpty()) {
            qCritical() << "Failed to find command for session:" << session;
            return;
        }

        // save session desktop file name, we'll use it to set the
        // last session later, in slotAuthenticationFinished()
        m_sessionName = session;

        QProcessEnvironment env;
        env.insert("PATH", daemonApp->configuration()->defaultPath());
        env.insert("DISPLAY", name());
        env.insert("XDG_SEAT", seat()->name());
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(QString("Session%1").arg(daemonApp->newSessionId())));
        env.insert("XDG_VTNR", QString::number(terminalId()));
        env.insert("DESKTOP_SESSION", sessionName);
        env.insert("XDG_CURRENT_DESKTOP", xdgSessionName);
        env.insert("XDG_SESSION_CLASS", "user");
        env.insert("XDG_SESSION_TYPE", m_displayServer->sessionType());
        env.insert("XDG_SESSION_DESKTOP", xdgSessionName);
        m_auth->insertEnvironment(env);

        m_auth->setUser(user);
        m_auth->setSession(command);
        m_auth->start();
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        if (success) {
            qDebug() << "Authenticated successfully";

#ifndef USE_WAYLAND
            struct passwd *pw = getpwnam(qPrintable(user));
            if (pw) {
                qobject_cast<XorgDisplayServer *>(m_displayServer)->addCookie(QString("%1/.Xauthority").arg(pw->pw_dir));
                changeOwner(QString("%1/.Xauthority").arg(pw->pw_dir));
            }
#endif

            // save last user and session
            daemonApp->configuration()->setLastUser(m_auth->user());
            daemonApp->configuration()->setLastSession(m_sessionName);
            daemonApp->configuration()->save();

            if (m_socket)
                emit loginSucceeded(m_socket);
        } else if (m_socket) {
            qDebug() << "Authentication failure";
            emit loginFailed(m_socket);
        }
        m_socket = nullptr;
    }

    void Display::slotAuthInfo(const QString &message, Auth::Info info) {
        // TODO: presentable to the user, eventually
        Q_UNUSED(info);
        qWarning() << "Authentication information:" << message;
    }

    void Display::slotAuthError(const QString &message, Auth::Error error) {
        // TODO: presentable to the user, eventually
        Q_UNUSED(error);
        qWarning() << "Authentication error:" << message;
    }

    void Display::slotHelperFinished(bool success) {
        stop();
    }

    void Display::slotRequestChanged() {
        if (m_auth->request()->prompts().length() == 1) {
            m_auth->request()->prompts()[0]->setResponse(qPrintable(m_passPhrase));
            m_auth->request()->done();
        } else if (m_auth->request()->prompts().length() == 2) {
            m_auth->request()->prompts()[0]->setResponse(qPrintable(m_auth->user()));
            m_auth->request()->prompts()[1]->setResponse(qPrintable(m_passPhrase));
            m_auth->request()->done();
        }
    }

    void Display::slotSessionStarted(bool success) {
        qDebug() << "Session started";
    }
}
