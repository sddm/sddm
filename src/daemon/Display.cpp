/***************************************************************************
* Copyright (c) 2014-2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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
#include "Seat.h"
#include "SocketServer.h"
#include "Utils.h"
#include "SignalHandler.h"
#include "VirtualTerminal.h"

#include <QDebug>
#include <QFile>
#include <QTimer>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Display::Display(Seat *parent) : QObject(parent),
        m_auth(new Auth(this)),
        m_displayAuth(new Auth(this)),
        m_seat(parent),
        m_socketServer(new SocketServer(this)) {

        // Always allocate a new vt
        m_terminalId = VirtualTerminal::setUpNewVt();

        // respond to authentication requests
        m_auth->setVerbose(true);
        connect(m_auth, SIGNAL(requestChanged()), this, SLOT(slotRequestChanged()));
        connect(m_auth, SIGNAL(authentication(QString,bool)), this, SLOT(slotAuthenticationFinished(QString,bool)));
        connect(m_auth, SIGNAL(session(bool)), this, SLOT(slotSessionStarted(bool)));
        connect(m_auth, SIGNAL(finished(Auth::HelperExitStatus)), this, SLOT(slotHelperFinished(Auth::HelperExitStatus)));
        connect(m_auth, SIGNAL(info(QString,Auth::Info)), this, SLOT(slotAuthInfo(QString,Auth::Info)));
        connect(m_auth, SIGNAL(error(QString,Auth::Error)), this, SLOT(slotAuthError(QString,Auth::Error)));

        // Display process
        m_displayAuth->setVerbose(true);
        m_displayAuth->setUser(QLatin1String("sddm"));
        m_displayAuth->setGreeter(true);
        connect(m_displayAuth, SIGNAL(session(bool)), this, SLOT(displayServerStarted()));
        connect(m_displayAuth, SIGNAL(finished(Auth::HelperExitStatus)), this, SLOT(stop()));

        // connect login signal
        connect(m_socketServer, SIGNAL(login(QLocalSocket*,QString,QString,Session)),
                this, SLOT(login(QLocalSocket*,QString,QString,Session)));

        // connect login result signals
        connect(this, SIGNAL(loginFailed(QLocalSocket*)), m_socketServer, SLOT(loginFailed(QLocalSocket*)));
        connect(this, SIGNAL(loginSucceeded(QLocalSocket*)), m_socketServer, SLOT(loginSucceeded(QLocalSocket*)));
    }

    Display::~Display() {
        stop();
    }

    const int Display::terminalId() const {
        return m_terminalId;
    }

    QString Display::sessionType() const {
        return QLatin1String("x11");
    }

    Seat *Display::seat() const {
        return m_seat;
    }

    void Display::start() {
        // check flag
        if (m_started)
            return;

        // start socket server
        m_socketServer->start(QLatin1String(":0"));
        if (!daemonApp->testing()) {
            // change the owner and group of the socket to avoid permission denied errors
            struct passwd *pw = getpwnam("sddm");
            if (pw) {
                if (chown(qPrintable(m_socketServer->socketAddress()), pw->pw_uid, pw->pw_gid) == -1) {
                    qWarning() << "Failed to change owner of the socket";
                    return;
                }
            }
        }

        // Display process
        QString cmd = QStringLiteral("%1/sddm-display").arg(QLatin1String(LIBEXEC_INSTALL_DIR));
        QStringList args;
        args << QStringLiteral("--socket") << m_socketServer->socketAddress();

        if (daemonApp->testing()) {
            args << QLatin1String("--test-mode");
        } else {
            // Switch to vt
            qDebug() << "Switching to vt" << m_terminalId << "for greeter";
            VirtualTerminal::jumpToVt(m_terminalId);

            // Process environment
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QLatin1String("XDG_SEAT"), m_seat->name());
            env.insert(QLatin1String("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(m_seat->name()));
            env.insert(QLatin1String("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
            env.insert(QLatin1String("XDG_SESSION_CLASS"), QLatin1String("greeter"));
            env.insert(QLatin1String("XDG_SESSION_TYPE"), QLatin1String("x11"));
            env.insert(QLatin1String("XDG_VTNR"), QString::number(m_terminalId));
            m_displayAuth->insertEnvironment(env);

            // Run display
            m_displayAuth->setSession(cmd + QLatin1Char(' ') + args.join(QLatin1Char(' ')));
            m_displayAuth->start();
        }
    }

    bool Display::attemptAutologin() {
        Session::Type sessionType = Session::X11Session;

        // determine session type
        QString autologinSession = mainConfig.Autologin.Session.get();
        // not configured: try last successful logged in
        if (autologinSession.isEmpty()) {
            autologinSession = stateConfig.Last.Session.get();
        }
        if (findSessionEntry(mainConfig.X11.SessionDir.get(), autologinSession)) {
            sessionType = Session::X11Session;
        } else if (findSessionEntry(mainConfig.Wayland.SessionDir.get(), autologinSession)) {
            sessionType = Session::WaylandSession;
        } else {
            qCritical() << "Unable to find autologin session entry" << autologinSession;
            return false;
        }

        Session session;
        session.setTo(sessionType, autologinSession);

        m_auth->setAutologin(true);
        startAuth(mainConfig.Autologin.User.get(), QString(), session);

        return true;
    }

    void Display::displayServerStarted() {
        // check flag
        if (m_started)
            return;

        // log message
        qDebug() << "Display server started.";

        if ((daemonApp->first || mainConfig.Autologin.Relogin.get()) &&
            !mainConfig.Autologin.User.get().isEmpty()) {
            // reset first flag
            daemonApp->first = false;

            // set flags
            m_started = true;

            bool success = attemptAutologin();
            if (success) {
                return;
            }
        }

        // reset first flag
        daemonApp->first = false;

        // set flags
        m_started = true;
    }

    void Display::stop() {
        // check flag
        if (!m_started)
            return;

        // stop the greeter
        if (daemonApp->testing()) {

        } else {
            //m_displayAuth->stop();
        }

        // stop socket server
        m_socketServer->stop();

        // reset flag
        m_started = false;

        // emit signal
        emit stopped();
    }

    void Display::login(QLocalSocket *socket,
                        const QString &user, const QString &password,
                        const Session &session) {
        m_socket = socket;

        //the SDDM user has special privileges that skip password checking so that we can load the greeter
        //block ever trying to log in as the SDDM user
        if (user == QLatin1String("sddm")) {
            return;
        }

        // authenticate
        startAuth(user, password, session);
    }

    bool Display::findSessionEntry(const QDir &dir, const QString &name) const {
        QString fileName = name;

        // append extension
        const QString extension = QStringLiteral(".desktop");
        if (!fileName.endsWith(extension))
            fileName += extension;

        return dir.exists(fileName);
    }

    void Display::startAuth(const QString &user, const QString &password, const Session &session) {
        m_passPhrase = password;

        // sanity check
        if (!session.isValid()) {
            qCritical() << "Invalid session" << session.fileName();
            return;
        }
        if (session.xdgSessionType().isEmpty()) {
            qCritical() << "Failed to find XDG session type for session" << session.fileName();
            return;
        }
        if (session.exec().isEmpty()) {
            qCritical() << "Failed to find command for session" << session.fileName();
            return;
        }

        // cache last session
        m_lastSession = session;

        // save session desktop file name, we'll use it to set the
        // last session later, in slotAuthenticationFinished()
        m_sessionName = session.fileName();

        // some information
        qDebug() << "Session" << m_sessionName << "selected, command:" << session.exec();

        // Always allocate a new VT because we run a separate Xorg server or Wayland compositor
        int vt = VirtualTerminal::setUpNewVt();
        m_lastSession.setVt(vt);

        QProcessEnvironment env;
        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        if (session.xdgSessionType() == QLatin1String("x11"))
            env.insert(QStringLiteral("DISPLAY"), m_display);
        env.insert(QStringLiteral("XDG_SEAT"), seat()->name());
        env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
        env.insert(QStringLiteral("XDG_VTNR"), QString::number(vt));
        env.insert(QStringLiteral("DESKTOP_SESSION"), session.desktopSession());
        env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), session.desktopNames());
        env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
        env.insert(QStringLiteral("XDG_SESSION_TYPE"), session.xdgSessionType());
        env.insert(QStringLiteral("XDG_SESSION_DESKTOP"), session.desktopNames());
        m_auth->insertEnvironment(env);

        m_auth->setUser(user);
        m_auth->setSession(session.exec());
        m_auth->start();
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        if (success) {
            qDebug() << "Authenticated successfully";

            m_auth->setCookie(m_cookie);

            // save last user and last session
            if (mainConfig.Users.RememberLastUser.get())
                stateConfig.Last.User.set(m_auth->user());
            else
                stateConfig.Last.User.setDefault();
            if (mainConfig.Users.RememberLastSession.get())
                stateConfig.Last.Session.set(m_sessionName);
            else
                stateConfig.Last.Session.setDefault();
            stateConfig.save();

            // switch to the new VT for user sessions
            qDebug() << "Switching to vt" << m_lastSession.vt() << "for user session";
            VirtualTerminal::jumpToVt(m_lastSession.vt());

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
        // TODO: handle more errors
        qWarning() << "Authentication error:" << message;

        if (!m_socket)
            return;

        if (error == Auth::ERROR_AUTHENTICATION)
            emit loginFailed(m_socket);
    }

    void Display::slotHelperFinished(Auth::HelperExitStatus status) {
        // Switch back to the greeter vt
        qDebug() << "Switch back to greeter vt" << m_terminalId;
        VirtualTerminal::jumpToVt(m_terminalId);
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
