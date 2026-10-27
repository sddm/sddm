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
#include "XorgDisplayServer.h"
#include "XorgUserDisplayServer.h"
#include "Seat.h"
#include "SocketServer.h"
#include "Greeter.h"
#include "Utils.h"

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QLocalSocket>

#include <pwd.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/ioctl.h>
#include <fcntl.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include "Login1Manager.h"
#include "Login1Session.h"
#include "VirtualTerminal.h"
#include "WaylandDisplayServer.h"
#include "config.h"

static int s_ttyFailures = 0;
#define STRINGIFY(x) #x

namespace SDDM {
    bool isTtyInUse(const QString &desiredTty) {
        if (Logind::isAvailable()) {
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
            auto reply = manager.ListSessions();
            reply.waitForFinished();

            const auto info = reply.value();
            for(const SessionInfo &s : info) {
                OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(), s.sessionPath.path(), QDBusConnection::systemBus());
                if (desiredTty == session.tTY() && session.state() != QLatin1String("closing")) {
                    qDebug() << "tty" << desiredTty << "already in use by" << session.user().path.path() << session.state()
                                      << session.display() << session.desktop() << session.vTNr();
                    return true;
                }
            }
        }
        return false;
    }

    int fetchAvailableVt() {
        if (!isTtyInUse(QStringLiteral("tty%1").arg(SDDM_INITIAL_VT))) {
            return SDDM_INITIAL_VT;
        }
        const auto vt = VirtualTerminal::currentVt();
        if (vt > 0 && !isTtyInUse(QStringLiteral("tty%1").arg(vt))) {
            return vt;
        }
        return VirtualTerminal::setUpNewVt();
    }

    Display::DisplayServerType Display::defaultDisplayServerType()
    {
        const QString &displayServerType = mainConfig.DisplayServer.get().toLower();
        DisplayServerType ret;
        if (displayServerType == QStringLiteral("x11-user")) {
            ret = X11UserDisplayServerType;
        } else if (displayServerType == QStringLiteral("wayland")) {
            ret = WaylandDisplayServerType;
        } else {
            if (displayServerType != QLatin1String("x11")) {
                qWarning("\"%s\" is an invalid value for General.DisplayServer: fall back to \"x11\"",
                    qPrintable(displayServerType));
            }
            ret = X11DisplayServerType;
        }
        return ret;
    }

    Display::Display(Seat *parent, DisplayServerType serverType)
        : QObject(parent),
        m_displayServerType(serverType),
        m_auth(new Auth(this)),
        m_fingerprintAuth(new Auth(this)),
        m_seat(parent),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this))
    {
        // Create display server
        switch (m_displayServerType) {
        case X11DisplayServerType:
            if (seat()->canTTY()) {
                m_terminalId = VirtualTerminal::setUpNewVt();
            }
            m_displayServer = new XorgDisplayServer(this);
            break;
        case X11UserDisplayServerType:
            if (seat()->canTTY()) {
                m_terminalId = fetchAvailableVt();
            }
            m_displayServer = new XorgUserDisplayServer(this);
            m_greeter->setDisplayServerCommand(XorgUserDisplayServer::command(this));
            break;
        case WaylandDisplayServerType:
            if (seat()->canTTY()) {
                m_terminalId = fetchAvailableVt();
            }
            m_displayServer = new WaylandDisplayServer(this);
            m_greeter->setDisplayServerCommand(mainConfig.Wayland.CompositorCommand.get());
            break;
        }

        qDebug("Using VT %d", m_terminalId);

        // respond to authentication requests
        for (Auth *auth : {m_auth, m_fingerprintAuth}) {
            auth->setVerbose(true);
            connect(auth, &Auth::requestChanged, this, &Display::slotRequestChanged);
            connect(auth, &Auth::authentication, this, &Display::slotAuthenticationFinished);
            connect(auth, &Auth::sessionStarted, this, &Display::slotSessionStarted);
            connect(auth, &Auth::finished, this, &Display::slotHelperFinished);
            connect(auth, &Auth::parallelUnavailable, this, [this] {
                if (m_parallelFailureNotified || m_selection.winner()) return;
                m_parallelFailureNotified = true;
                m_fingerprintRetry.stop();
                m_selection.selectUser(QString());
                m_passPhrase.clear();
                if (m_socket) {
                    emit loginFailed(m_socket);
                }
            });
            connect(auth, &Auth::info, this, &Display::slotAuthInfo);
            connect(auth, &Auth::error, this, &Display::slotAuthError);
        }
        m_fingerprintAuth->setFingerprint(true);
        m_fingerprintRetry.setSingleShot(true);
        m_fingerprintRetry.setInterval(2000);
        connect(&m_fingerprintRetry, &QTimer::timeout, this, &Display::retryFingerprint);
        connect(m_socketServer, &SocketServer::startFingerprint, this, &Display::startFingerprint);
        connect(m_socketServer, &SocketServer::disconnected, this, [this](QLocalSocket *socket) {
            if (m_socket == socket && !m_selection.winner()) {
                m_socket = nullptr;
                m_fingerprintRetry.stop();
                m_selection.selectUser(QString());
                m_auth->cancelAuthentication();
                m_fingerprintAuth->cancelAuthentication();
            }
        });

        // restart display after display server ended
        connect(m_displayServer, &DisplayServer::started, this, &Display::displayServerStarted);
        connect(m_displayServer, &DisplayServer::stopped, this, &Display::stop);

        // connect login signal
        connect(m_socketServer, &SocketServer::login, this, &Display::login);

        // connect login result signals
        connect(this, &Display::loginFailed, m_socketServer, &SocketServer::loginFailed);
        connect(this, &Display::loginSucceeded, m_socketServer, &SocketServer::loginSucceeded);

        connect(m_greeter, &Greeter::failed, this, &Display::stop);
        connect(m_greeter, &Greeter::ttyFailed, this, [this] {
            ++s_ttyFailures;
            if (s_ttyFailures > 5) {
                QCoreApplication::exit(23);
            }
            // It might be the case that we are trying a tty that has been taken over by a
            // different process. In such a case, switch back to the initial one and try again.
            VirtualTerminal::jumpToVt(SDDM_INITIAL_VT, true);
            stop();
        });
        connect(m_greeter, &Greeter::displayServerFailed, this, &Display::displayServerFailed);

        // Load autologin configuration (whether to autologin, user, session, session type)
        if ((daemonApp->first || mainConfig.Autologin.Relogin.get()) &&
            !mainConfig.Autologin.User.get().isEmpty()) {
            // determine session type
            QString autologinSession = mainConfig.Autologin.Session.get();
            // not configured: try last successful logged in
            if (autologinSession.isEmpty()) {
                autologinSession = stateConfig.Last.Session.get();
            }
            if (findSessionEntry(mainConfig.Wayland.SessionDir.get(), autologinSession)) {
                m_autologinSession.setTo(Session::WaylandSession, autologinSession);
            } else if (findSessionEntry(mainConfig.X11.SessionDir.get(), autologinSession)) {
                m_autologinSession.setTo(Session::X11Session, autologinSession);
            } else {
                qCritical() << "Unable to find autologin session entry" << autologinSession;
            }
        }

        // reset first flag
        daemonApp->first = false;
    }

    Display::~Display() {
        disconnect(m_auth, &Auth::finished, this, &Display::slotHelperFinished);
        disconnect(m_fingerprintAuth, &Auth::finished, this, &Display::slotHelperFinished);
        stop();
    }

    Display::DisplayServerType Display::displayServerType() const
    {
        return m_displayServerType;
    }

    DisplayServer *Display::displayServer() const
    {
        return m_displayServer;
    }

    int Display::terminalId() const {
        return (m_auth->isActive() || m_fingerprintAuth->isActive()) ? m_sessionTerminalId : m_terminalId;
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

    bool Display::start() {
        if (m_started)
            return true;

        m_started = true;

        // Handle autologin early, unless it needs the display server to be up
        // (rootful X + X11 autologin session).
        if (m_autologinSession.isValid()
            && !(m_displayServerType == X11DisplayServerType
                 && m_autologinSession.type() == Session::X11Session)) {
            m_auth->setAutologin(true);
            if (startAuth(mainConfig.Autologin.User.get(), QString(), m_autologinSession))
                return true;
            else
                return handleAutologinFailure();
         }

        return m_displayServer->start();
    }

    void Display::startSocketServerAndGreeter() {
        // start socket server
        m_socketServer->start(m_displayServer->display());

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

        m_greeter->setSocket(m_socketServer->socketAddress());
        m_greeter->setTheme(findGreeterTheme());

        // start greeter
        m_greeter->start();
    }

    bool Display::handleAutologinFailure() {
        qWarning() << "Autologin failed!";
        m_auth->setAutologin(false);
        // For late autologin handling only the greeter needs to be started.
        if (m_displayServerType == X11DisplayServerType
            && m_autologinSession.type() == Session::X11Session) {
            startSocketServerAndGreeter();
            return true;
        } else {
            return m_displayServer->start();
        }
    }

    void Display::displayServerStarted() {
        // setup display
        m_displayServer->setupDisplay();

        // log message
        qDebug() << "Display server started.";

        // Handle autologin late if it needs the display server to be up
        // (rootful X + X11 autologin session).
        if (m_autologinSession.isValid()
            && (m_displayServerType == X11DisplayServerType
                && m_autologinSession.type() == Session::X11Session)) {
            m_auth->setAutologin(true);
            if (!startAuth(mainConfig.Autologin.User.get(), QString(), m_autologinSession))
                handleAutologinFailure();

            return;
        }

        startSocketServerAndGreeter();
    }

    void Display::stop() {
        if (!m_started || m_stopping) return;
        m_stopping = true;
        m_selection.close();
        m_fingerprintRetry.stop();

        // stop the greeter
        m_greeter->stop();

        m_auth->stop();
        m_fingerprintAuth->stop();

        // stop socket server
        m_socketServer->stop();

        // stop display server
        m_displayServer->blockSignals(true);
        m_displayServer->stop();
        m_displayServer->blockSignals(false);

        // reset flag
        m_started = false;

        // emit signal
        emit stopped();
    }

    void Display::login(QLocalSocket *socket,
                        const QString &user, const QString &password,
                        const Session &session) {
        if (m_stopping || m_selection.winner() || m_legacySessionReleased) return;
        if (m_socket && m_socket != socket) return;
        m_socket = socket;

        //the SDDM user has special privileges that skip password checking so that we can load the greeter
        //block ever trying to log in as the SDDM user
        if (user == QLatin1String("sddm")) {
            emit loginFailed(m_socket);
            return;
        }

        if (parallelFingerprintEnabled()) {
            m_parallelFailureNotified = false;
            // A password submission never cancels the fingerprint channel.
            if (m_selection.user() != user && (m_auth->isActive() || m_fingerprintAuth->isActive())) {
                emit loginFailed(socket);
                return;
            }
            m_selection.selectUser(user);
        }
        if (!startAuth(user, password, session)) emit loginFailed(socket);
    }

    void Display::startFingerprint(QLocalSocket *socket, const QString &user, const Session &session) {
        if (!parallelFingerprintEnabled() || m_stopping || m_selection.winner() || m_legacySessionReleased ||
            !m_greeter->isRunning()) return;
        if (m_socket && m_socket != socket) return;
        m_parallelFailureNotified = false;
        // Runtime readiness is withheld while a legacy helper is alive.
        if (m_auth->isActive() && !m_auth->parallelAttempt()) return;
        m_socket = socket;
        if (user.isEmpty() || user == QLatin1String("sddm") || !session.isValid() ||
            session.exec().isEmpty() || session.xdgSessionType().isEmpty()) {
            m_fingerprintRetry.stop();
            m_selection.selectUser(QString());
            m_fingerprintSession = Session();
            m_preparedUser.clear();
            m_passPhrase.clear();
            m_auth->cancelAuthentication();
            m_fingerprintAuth->cancelAuthentication();
            m_socketServer->fingerprintActive(socket, false);
            return;
        }
        // The first fingerprint request may arrive after a password was submitted.
        // Starting the other channel for that same selection must not cancel it.
        const Session &selected = m_fingerprintSession.isValid() ? m_fingerprintSession : m_preparedSession;
        const bool changed = (!m_selection.user().isEmpty() && m_selection.user() != user) ||
            (selected.isValid() && (selected.fileName() != session.fileName() ||
             selected.exec() != session.exec() || selected.type() != session.type()));
        if (changed) {
            m_auth->cancelAuthentication();
            m_fingerprintAuth->cancelAuthentication();
        }
        m_selection.selectUser(user);
        m_fingerprintSession = session;
        retryFingerprint();
    }

    void Display::retryFingerprint() {
        if (!parallelFingerprintEnabled() || m_stopping || m_selection.winner() ||
            !m_socket || m_selection.user().isEmpty() || !m_greeter->isRunning()) return;
        if (m_fingerprintAuth->isActive()) return;
        if (!startAuth(m_selection.user(), QString(), m_fingerprintSession, m_fingerprintAuth)) {
            m_fingerprintRetry.start();
            return;
        }
        }

    QString Display::findGreeterTheme() const {
        QString themeName = mainConfig.Theme.Current.get();

        // an unconfigured theme means the user wants to load the
        // default theme from the resources
        if (themeName.isEmpty())
            return QString();

        QDir dir(mainConfig.Theme.ThemeDir.get());

        // return the default theme if it exists
        if (dir.exists(themeName))
            return dir.absoluteFilePath(themeName);

        // otherwise use the embedded theme
        qWarning() << "The configured theme" << themeName << "doesn't exist, using the embedded theme instead";
        return QString();
    }

    bool Display::findSessionEntry(const QStringList &dirPaths, const QString &name) const {
        const QFileInfo fileInfo(name);
        QString fileName = name;

        // append extension
        const QString extension = QStringLiteral(".desktop");
        if (!fileName.endsWith(extension))
            fileName += extension;

        for (const auto &path: dirPaths) {
            QDir dir = path;

            // Given an absolute path: Check that it matches dir
            if (fileInfo.isAbsolute() && fileInfo.absolutePath() != dir.absolutePath())
                continue;

            if (dir.exists(fileName))
                return true;
        }
        return false;
    }

    bool Display::startAuth(const QString &user, const QString &password, const Session &session, Auth *auth) {
        if (!auth) auth = m_auth;
        const bool parallel = parallelFingerprintEnabled();
        auth->setPasswordOnly(auth == m_auth && parallel && !auth->autologin());
        if (m_stopping || m_selection.winner()) return false;
        if (auth->isActive()) {
            qWarning() << "Existing authentication ongoing, aborting";
            return false;
        }

        if (auth == m_auth) m_passPhrase = password;

        // sanity check
        if (!session.isValid()) {
            qCritical() << "Invalid session" << session.fileName();
            return false;
        }
        if (session.xdgSessionType().isEmpty()) {
            qCritical() << "Failed to find XDG session type for session" << session.fileName();
            return false;
        }
        if (session.exec().isEmpty()) {
            qCritical() << "Failed to find command for session" << session.fileName();
            return false;
        }

        const bool sameContext = m_preparedUser == user &&
            m_preparedSession.fileName() == session.fileName() &&
            m_preparedSession.exec() == session.exec() && m_preparedSession.type() == session.type();
        Auth *other = auth == m_auth ? m_fingerprintAuth : m_auth;
        if (parallelFingerprintEnabled() && sameContext) {
            auth->setUser(user);
            auth->setSession(m_reuseSessionId.isNull() ? session.exec() : QString());
            auth->setDisplayServerCommand(session.xdgSessionType() == QLatin1String("x11") &&
                m_displayServerType != X11DisplayServerType ? XorgUserDisplayServer::command(this) : QString());
            auth->insertEnvironment(m_preparedEnvironment);
            auth->start();
            return true;
        }
        if (parallelFingerprintEnabled() && other->isActive()) return false;

        m_reuseSessionId = QString();

        if (Logind::isAvailable() && mainConfig.Users.ReuseSession.get()) {
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
            auto reply = manager.ListSessions();
            reply.waitForFinished();

            const auto info = reply.value();
            for(const SessionInfo &s : reply.value()) {
                if (s.userName == user) {
                    OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(), s.sessionPath.path(), QDBusConnection::systemBus());
                    if ((session.service() == QLatin1String("sddm") ||
                         session.service() == QLatin1String("sddm-autologin") ||
                         session.service() == QLatin1String("sddm-password") ||
                         session.service() == QLatin1String("sddm-fingerprint")) &&
                        session.state() == QLatin1String("online")) {
                        m_reuseSessionId = s.sessionId;
                        break;
                    }
                }
            }
        }

        // save session desktop file name, we'll use it to set the
        // last session later, in slotAuthenticationFinished()
        m_sessionName = session.fileName();

        m_sessionTerminalId = m_terminalId;
        if ((session.type() == Session::WaylandSession && m_displayServerType == X11DisplayServerType) || (m_greeter->isRunning() && m_displayServerType != X11DisplayServerType)) {
            // Create a new VT when we need to have another compositor running
            if (seat()->canTTY()) {
                m_sessionTerminalId = VirtualTerminal::setUpNewVt();
            }
        }

        // some information
        qDebug() << "Session" << m_sessionName << "selected, command:" << session.exec() << "for VT" << m_sessionTerminalId;

        QProcessEnvironment env;
        env.insert(session.additionalEnv());

        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
        env.insert(QStringLiteral("DESKTOP_SESSION"), session.desktopSession());
        if (!session.desktopNames().isEmpty())
            env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), session.desktopNames());
        env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
        env.insert(QStringLiteral("XDG_SESSION_TYPE"), session.xdgSessionType());
        env.insert(QStringLiteral("XDG_SEAT"), seat()->name());
        if (m_sessionTerminalId > 0)
            env.insert(QStringLiteral("XDG_VTNR"), QString::number(m_sessionTerminalId));
#ifdef HAVE_SYSTEMD
        env.insert(QStringLiteral("XDG_SESSION_DESKTOP"), session.desktopNames());
#endif

        if (session.xdgSessionType() == QLatin1String("x11")) {
          if (m_displayServerType == X11DisplayServerType)
            env.insert(QStringLiteral("DISPLAY"), name());
          else
            auth->setDisplayServerCommand(XorgUserDisplayServer::command(this));
        } else {
            auth->setDisplayServerCommand(QStringLiteral());
	}
        auth->setUser(user);
        if (m_reuseSessionId.isNull()) {
            auth->setSession(session.exec());
        }
        m_preparedUser = user;
        m_preparedSession = session;
        m_preparedEnvironment = env;
        auth->insertEnvironment(env);
        auth->start();

        return true;
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth) return;
        if (auth->parallelAttempt() && !auth->autologin()) {
            if (!parallelFingerprintEnabled()) { auth->setSessionApproved(false); return; }
            if (success) {
                const bool accepted = !m_stopping && m_selection.accept(auth, user);
                auth->setSessionApproved(accepted);
                if (!accepted) return;
                m_fingerprintRetry.stop();
                (auth == m_auth ? m_fingerprintAuth : m_auth)->cancelAuthentication();
                m_passPhrase.clear();
            } else {
                // Child completion controls retry/UI failure; no credential replay.
                return;
            }
        }
        if (auth->autologin() && !success) {
            handleAutologinFailure();
            return;
        }

        if (success) {
            if (!auth->parallelAttempt()) m_legacySessionReleased = true;
            qDebug() << "Authentication for user " << user << " successful";

            if (!m_reuseSessionId.isNull()) {
                OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
                manager.UnlockSession(m_reuseSessionId);
                manager.ActivateSession(m_reuseSessionId);
            } else {
                if (qobject_cast<XorgDisplayServer *>(m_displayServer))
                    auth->setCookie(qobject_cast<XorgDisplayServer *>(m_displayServer)->cookie());
            }

            // save last user and last session
            if (mainConfig.Users.RememberLastUser.get())
                stateConfig.Last.User.set(auth->user());
            else
                stateConfig.Last.User.setDefault();
            if (mainConfig.Users.RememberLastSession.get())
                stateConfig.Last.Session.set(m_sessionName);
            else
                stateConfig.Last.Session.setDefault();
            stateConfig.save();

            if (m_socket)
                emit loginSucceeded(m_socket);
        } else if (m_socket) {
            qDebug() << "Authentication for user " << user << " failed";
            emit loginFailed(m_socket);
        }
        m_socket = nullptr;
    }

    void Display::slotAuthInfo(const QString &message, Auth::Info info) {
        // An opt-in PAM notification is sent only after VerifyStart succeeds.
        if (message == QStringLiteral("\036FPRINTD_VERIFY_READY_V1")) {
            Auth *auth = qobject_cast<Auth *>(sender());
            if (auth == m_fingerprintAuth && auth->parallelAttempt() && auth->isActive() &&
                !auth->cancelled() && !m_selection.user().isEmpty() &&
                auth->user() == m_selection.user() && !m_stopping && !m_selection.winner() && m_socket)
                m_socketServer->fingerprintActive(m_socket, true);
            return;
        }
        qWarning() << "Authentication information:" << info << message;

        if (!m_socket)
            return;

        m_socketServer->informationMessage(m_socket, message);
    }

    void Display::slotAuthError(const QString &message, Auth::Error error) {
        qWarning() << "Authentication error:" << error << message;

        if (!m_socket)
            return;

        m_socketServer->informationMessage(m_socket, message);
        if (error == Auth::ERROR_AUTHENTICATION && !parallelFingerprintEnabled())
            emit loginFailed(m_socket);
    }

    void Display::slotHelperFinished(Auth::HelperExitStatus status) {
        if (m_stopping) return;
        Auth *auth = qobject_cast<Auth *>(sender());
        if (auth && auth->parallelAttempt() && !auth->autologin()) {
            if (!parallelFingerprintEnabled()) return;
            if (m_selection.winner()) {
                if (auth == m_selection.winner()) stop();
                return;
            }
            if (auth == m_fingerprintAuth) {
                if (m_socket) m_socketServer->fingerprintActive(m_socket, false);
                if (m_socket && !m_selection.user().isEmpty()) m_fingerprintRetry.start();
            } else {
                m_passPhrase.clear();
                if (m_socket) emit loginFailed(m_socket);
            }
            return;
        }
        // Don't restart greeter and display server unless sddm-helper exited
        // with an internal error or the user session finished successfully,
        // we want to avoid greeter from restarting when an authentication
        // error happens (in this case we want to show the message from the
        // greeter
        if (status != Auth::HELPER_AUTH_ERROR)
            stop();
    }

    void Display::slotRequestChanged() {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth) return;
        // The fingerprint-only stack must never consume a password. Refuse unexpected prompts.
        if (auth->fingerprint() && !auth->request()->prompts().isEmpty()) {
            auth->cancelAuthentication();
            return;
        }
        if (auth->request()->prompts().length() == 1) {
            auth->request()->prompts()[0]->setResponse(qPrintable(m_passPhrase));
            auth->request()->done();
        } else if (auth->request()->prompts().length() == 2) {
            auth->request()->prompts()[0]->setResponse(qPrintable(auth->user()));
            auth->request()->prompts()[1]->setResponse(qPrintable(m_passPhrase));
            auth->request()->done();
        }
    }

    void Display::slotSessionStarted(bool success) {
        qDebug() << "Session started" << success;
        if (success) {
            QTimer::singleShot(5000, m_greeter, &Greeter::stop);
        }
    }
}
