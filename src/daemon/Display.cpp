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
#include <QUuid>
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
        m_auth->setVerbose(true);
        connect(m_auth, &Auth::requestChanged, this, &Display::slotRequestChanged);
        connect(m_auth, &Auth::authentication, this, &Display::slotAuthenticationFinished);
        connect(m_auth, &Auth::sessionStarted, this, &Display::slotSessionStarted);
        connect(m_auth, &Auth::finished, this, &Display::slotHelperFinished);
        connect(m_auth, &Auth::info, this, &Display::slotAuthInfo);
        connect(m_auth, &Auth::error, this, &Display::slotAuthError);
        m_fingerprintAuth->setVerbose(true);
        connect(m_fingerprintAuth, &Auth::requestChanged, this, &Display::slotRequestChanged);
        connect(m_fingerprintAuth, &Auth::authentication, this, &Display::slotAuthenticationFinished);
        connect(m_fingerprintAuth, &Auth::sessionStarted, this, &Display::slotSessionStarted);
        connect(m_fingerprintAuth, &Auth::finished, this, &Display::slotHelperFinished);
        connect(m_fingerprintAuth, &Auth::info, this, &Display::slotAuthInfo);
        connect(m_fingerprintAuth, &Auth::error, this, &Display::slotAuthError);

        // restart display after display server ended
        connect(m_displayServer, &DisplayServer::started, this, &Display::displayServerStarted);
        connect(m_displayServer, &DisplayServer::stopped, this, &Display::stop);

        // connect login signal
        connect(m_socketServer, &SocketServer::login, this, &Display::login);
        connect(m_socketServer, &SocketServer::startFingerprint, this, &Display::startFingerprint);
        connect(m_socketServer, &SocketServer::cancelFingerprint, this, &Display::cancelFingerprint);

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
        // Destruction can run after the main-thread event dispatcher is gone.
        for (Auth *auth : {m_auth, m_fingerprintAuth})
            disconnect(auth, nullptr, this, nullptr);
        stopInternal(true);
        for (Auth *auth : {m_auth, m_fingerprintAuth})
            auth->stop();
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
        stopInternal(false);
    }

    void Display::stopInternal(bool destroying) {
        // check flag
        if (!m_started)
            return;

        m_pendingLogin = false;
        m_pendingPassword.clear();
        m_pendingSocket.clear();
        cancelMethodAttempt(destroying);

        // stop the greeter
        m_greeter->stop();

        auto stopHelper = [destroying](Auth *auth) {
            if (!auth->isActive())
                return;
            auth->authorizeSession(false);
            if (destroying) {
                auth->stop();
                return;
            }
            QPointer<Auth> guarded(auth);
            const quint64 generation = auth->launchGeneration();
            QTimer::singleShot(5000, auth, [guarded, generation] {
                if (guarded && guarded->launchGeneration() == generation && guarded->isActive())
                    guarded->kill();
            });
        };
        stopHelper(m_auth);
        stopHelper(m_fingerprintAuth);

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
        if (m_methodAttemptActive && socket != m_attemptSocket) {
            emit loginFailed(socket);
            return;
        }
        m_socket = socket;

        //the SDDM user has special privileges that skip password checking so that we can load the greeter
        //block ever trying to log in as the SDDM user
        if (user == QLatin1String("sddm")) {
            emit loginFailed(m_socket);
            return;
        }

        if (m_methodAttemptActive) {
            if (m_methodWinnerChosen) {
                return;
            }
            if (user != m_attemptUser || session.type() != m_attemptSession.type()
                || session.fileName() != m_attemptSession.fileName()) {
                m_pendingLogin = true;
                m_pendingSocket = socket;
                m_pendingUser = user;
                m_pendingPassword = password;
                m_pendingSession = session;
                cancelMethodAttempt();
                startPendingLogin();
                return;
            }
            if (m_auth->isActive()) {
                return;
            }
        } else {
            m_requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_attemptUser = user;
            m_attemptSession = session;
            m_attemptSocket = socket;
            m_methodAttemptActive = true;
            m_methodWinnerChosen = false;
            m_attemptGate.begin(m_requestId, user);
            m_waitingForLoser = false;
            const QString requestId = m_requestId;
            connect(socket, &QLocalSocket::disconnected, this, [this, requestId] {
                if (m_methodAttemptActive && m_requestId == requestId)
                    cancelMethodAttempt();
            });
        }
        if (!startAuth(user, password, session, AuthMethod::Password)) {
            emit loginFailed(m_attemptSocket);
            finishMethodAttempt(false);
        }
    }

    void Display::startFingerprint(QLocalSocket *socket, const QString &user, const Session &session) {
        if (user.isEmpty() || user == QLatin1String("sddm") || !session.isValid()
            || !m_started || (m_fingerprintAuth->isActive() && !m_methodAttemptActive)) {
            emit loginFailed(socket);
            return;
        }
        if (m_methodAttemptActive) {
            if (socket != m_attemptSocket)
                return;
            if (user != m_attemptUser || session.type() != m_attemptSession.type()
                || session.fileName() != m_attemptSession.fileName()) {
                cancelMethodAttempt();
                return;
            }
            if (m_methodWinnerChosen) {
                m_socketServer->authenticationMethodState(socket, m_requestId, 4, QStringLiteral("matched"));
                return;
            }
            if (m_fingerprintAuth->isActive()) {
                m_socketServer->authenticationMethodState(socket, m_requestId, 1, QStringLiteral("scanning"));
                return;
            }
            if (!startAuth(user, QString(), session, AuthMethod::Fingerprint))
                m_socketServer->authenticationMethodState(socket, m_requestId, 5, QStringLiteral("failed"));
            return;
        }

        m_socket = socket;
        m_requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_attemptUser = user;
        m_attemptSession = session;
        m_attemptSocket = socket;
        m_methodAttemptActive = true;
        m_methodWinnerChosen = false;
        m_attemptGate.begin(m_requestId, user);
        m_waitingForLoser = false;
        const QString requestId = m_requestId;
        connect(socket, &QLocalSocket::disconnected, this, [this, requestId] {
            if (m_methodAttemptActive && m_requestId == requestId)
                cancelMethodAttempt();
        });
        if (!startAuth(user, QString(), session, AuthMethod::Fingerprint)) {
            finishMethodAttempt(false);
            return;
        }
        m_socketServer->authenticationMethodState(socket, m_requestId, 1, QStringLiteral("scanning"));
    }

    void Display::cancelFingerprint(QLocalSocket *socket, const QString &requestId) {
        if (!m_methodAttemptActive || m_methodWinnerChosen || socket != m_attemptSocket
            || requestId != m_requestId || !m_fingerprintAuth->isActive()
            || m_fingerprintAuth->requestId() != requestId)
            return;
        m_attemptGate.cancel(AuthMethod::Fingerprint);
        m_cancelledHelpers.insert(m_fingerprintAuth);
        m_fingerprintAuth->authorizeSession(false);
        QPointer<Auth> guarded(m_fingerprintAuth);
        const quint64 generation = m_fingerprintAuth->launchGeneration();
        QTimer::singleShot(5000, m_fingerprintAuth, [guarded, generation] {
            if (guarded && guarded->launchGeneration() == generation && guarded->isActive())
                guarded->kill();
        });
        m_fingerprintAuth->terminate();
    }

    void Display::cancelMethodAttempt(bool destroying) {
        if (!m_methodAttemptActive)
            return;
        const QString requestId = m_requestId;
        QPointer<QLocalSocket> socket = m_attemptSocket;
        m_attemptGate.invalidate();
        m_methodAttemptActive = false;
        m_methodWinnerChosen = false;
        m_waitingForLoser = false;
        if (socket) {
            m_socketServer->authenticationMethodState(socket, requestId, 6, QStringLiteral("cancelled"));
            m_socketServer->authenticationAttemptFinished(socket, requestId, false, 0);
        }
        for (Auth *auth : {m_auth, m_fingerprintAuth}) {
            if (!auth->isActive() || auth->requestId() != requestId)
                continue;
            m_cancelledHelpers.insert(auth);
            auth->authorizeSession(false);
            if (destroying) {
                auth->stop();
            } else {
                auth->terminate();
                QPointer<Auth> guarded(auth);
                const quint64 generation = auth->launchGeneration();
                QTimer::singleShot(5000, auth, [guarded, generation] {
                    if (guarded && guarded->launchGeneration() == generation && guarded->isActive())
                        guarded->kill();
                });
            }
        }
        m_winnerAuth.clear();
        m_loserAuth.clear();
        m_attemptReuseSessionId.clear();
        m_reuseSessionSelected = false;
        m_attemptSocket.clear();
    }

    void Display::authorizeWinner() {
        if (!m_methodAttemptActive || !m_winnerAuth || m_waitingForLoser
            || !m_attemptGate.authorize(m_winnerAuth->method()))
            return;
        if (qobject_cast<XorgDisplayServer *>(m_displayServer))
            m_winnerAuth->setCookie(qobject_cast<XorgDisplayServer *>(m_displayServer)->cookie());
        m_winnerAuth->authorizeSession(true);
        m_socketServer->authenticationMethodState(m_attemptSocket, m_requestId, 4, QStringLiteral("matched"));
    }

    void Display::startPendingLogin() {
        if (!m_pendingLogin || m_auth->isActive() || m_fingerprintAuth->isActive())
            return;
        QPointer<QLocalSocket> socket = m_pendingSocket;
        const QString user = m_pendingUser;
        const QString password = m_pendingPassword;
        Session session;
        session = m_pendingSession;
        m_pendingLogin = false;
        m_pendingSocket.clear();
        m_pendingUser.clear();
        m_pendingPassword.clear();
        m_pendingSession = Session();
        if (socket && socket->state() == QLocalSocket::ConnectedState)
            login(socket, user, password, session);
    }

    void Display::finishMethodAttempt(bool success) {
        if (!m_methodAttemptActive)
            return;
        const auto socket = m_attemptSocket;
        const QString requestId = m_requestId;
        const quint32 method = m_winnerAuth ? qint32(m_winnerAuth->method()) : 0;
        m_socketServer->authenticationMethodState(socket, requestId, success ? 4 : 5,
                                                   success ? QStringLiteral("matched") : QStringLiteral("failed"));
        m_socketServer->authenticationAttemptFinished(socket, requestId, success, method);
        m_methodAttemptActive = false;
        m_methodWinnerChosen = false;
        m_attemptGate.invalidate();
        m_waitingForLoser = false;
        m_winnerAuth.clear();
        m_loserAuth.clear();
        m_attemptSocket.clear();
        m_attemptReuseSessionId.clear();
        m_reuseSessionSelected = false;
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

    bool Display::startAuth(const QString &user, const QString &password, const Session &session, AuthMethod method) {
        Auth *auth = method == AuthMethod::Fingerprint ? m_fingerprintAuth : m_auth;
        if (auth->isActive()) {
            qWarning() << "Existing authentication method is still active";
            return false;
        }
        if (!m_methodAttemptActive && (m_auth->isActive() || m_fingerprintAuth->isActive())) {
            qWarning() << "Existing authentication ongoing, aborting";
            return false;
        }

        if (method == AuthMethod::Password)
            m_passPhrase = password;
        auth->setMethod(method);
        auth->setRequestId(m_methodAttemptActive ? m_requestId : QUuid::createUuid().toString(QUuid::WithoutBraces));
        auth->setDeferredAuthorization(m_methodAttemptActive);

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

        m_reuseSessionId = QString();

        if (Logind::isAvailable() && mainConfig.Users.ReuseSession.get()) {
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
            auto reply = manager.ListSessions();
            reply.waitForFinished();

            const auto info = reply.value();
            for(const SessionInfo &s : reply.value()) {
                if (s.userName == user) {
                    OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(), s.sessionPath.path(), QDBusConnection::systemBus());
                    const QString service = session.service();
                    if ((service == QLatin1String("sddm")
                        || service == QLatin1String("sddm-autologin")
                        || service == QLatin1String("sddm-fingerprint"))
                        && session.state() == QLatin1String("online")) {
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
        if (m_methodAttemptActive && !m_reuseSessionSelected) {
            m_attemptReuseSessionId = m_reuseSessionId;
            m_reuseSessionSelected = true;
        }
        auth->setSession((m_methodAttemptActive ? m_attemptReuseSessionId : m_reuseSessionId).isNull()
                             ? session.exec() : QString());
        if (m_methodAttemptActive)
            m_attemptGate.resume(method);
        auth->insertEnvironment(env);
        auth->start();

        return true;
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (auth && (m_cancelledHelpers.contains(auth) || m_attemptGate.cancelled(auth->method()))) {
            auth->authorizeSession(false);
            return;
        }
        if (m_methodAttemptActive && auth && auth->requestId() == m_requestId) {
            if (!success) {
                if (auth->method() == AuthMethod::Fingerprint)
                    m_socketServer->authenticationMethodState(m_attemptSocket, m_requestId, 5, QStringLiteral("failed"));
                return; // helper exit reports password failure; keep the other method available
            }
            if (m_methodWinnerChosen && auth == m_winnerAuth && user == m_attemptUser)
                return;
            if (!m_attemptGate.select(auth->requestId(), user, auth->method())) {
                auth->authorizeSession(false);
                auth->terminate();
                return;
            }

            m_methodWinnerChosen = true;
            m_winnerAuth = auth;
            Auth *other = auth == m_auth ? m_fingerprintAuth : m_auth;
            if (other->isActive() && other->requestId() == m_requestId) {
                m_loserAuth = other;
                m_attemptGate.waitFor(other->method());
                m_waitingForLoser = true;
                other->authorizeSession(false);
                other->terminate();
                QPointer<Auth> guarded(other);
                const quint64 generation = other->launchGeneration();
                QTimer::singleShot(5000, other, [guarded, generation] {
                    if (guarded && guarded->launchGeneration() == generation && guarded->isActive())
                        guarded->kill();
                });
            } else {
                authorizeWinner();
            }
            return;
        }
        // Deferred method helpers must never fall through to stock password handling,
        // including after cancellation or when their request is stale.
        if (auth && auth->isDeferredAuthorization()) {
            auth->authorizeSession(false);
            return;
        }

        if (m_auth->autologin() && !success) {
            handleAutologinFailure();
            return;
        }

        if (success) {
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
                stateConfig.Last.User.set(m_auth->user());
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
        qWarning() << "Authentication information:" << info << message;
        Auth *auth = qobject_cast<Auth *>(sender());
        QLocalSocket *socket = m_methodAttemptActive ? m_attemptSocket.data() : m_socket.data();
        if (!socket)
            return;
        if (m_methodAttemptActive && auth && auth->method() == AuthMethod::Fingerprint) {
            m_socketServer->authenticationMethodState(socket, m_requestId, 1, QStringLiteral("scanning"));
            return;
        }
        m_socketServer->informationMessage(socket, message);
    }

    void Display::slotAuthError(const QString &message, Auth::Error error) {
        qWarning() << "Authentication error:" << error << message;
        Auth *auth = qobject_cast<Auth *>(sender());
        QLocalSocket *socket = m_methodAttemptActive ? m_attemptSocket.data() : m_socket.data();
        if (!socket)
            return;
        if (auth && auth->isDeferredAuthorization()) {
            if (m_methodAttemptActive && auth->requestId() == m_requestId
                && auth->method() == AuthMethod::Fingerprint)
                m_socketServer->authenticationMethodState(socket, m_requestId, 5, QStringLiteral("failed"));
            else if (m_methodAttemptActive && auth->requestId() == m_requestId
                     && auth->method() == AuthMethod::Password)
                m_socketServer->informationMessage(socket, message);
            return;
        }
        m_socketServer->informationMessage(socket, message);
        if (m_methodAttemptActive && auth == m_winnerAuth)
            return; // the helper-exit path reports a post-authorization failure once
        if (error == Auth::ERROR_AUTHENTICATION)
            emit loginFailed(socket);
    }

    void Display::slotHelperFinished(Auth::HelperExitStatus status) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (auth && auth == m_sessionAuth) {
            m_sessionAuth.clear();
            stop();
            return;
        }
        if (m_cancelledHelpers.remove(auth)) {
            if (auth && auth->method() == AuthMethod::Fingerprint && m_methodAttemptActive
                && auth->requestId() == m_requestId && m_attemptSocket)
                m_socketServer->authenticationMethodState(m_attemptSocket, m_requestId, 6, QStringLiteral("cancelled"));
            if (auth && auth == m_loserAuth && m_methodAttemptActive && auth->requestId() == m_requestId) {
                m_loserAuth.clear();
                m_attemptGate.exited(auth->method());
                m_waitingForLoser = false;
                authorizeWinner();
            }
            startPendingLogin();
            return;
        }
        if (auth && auth->isDeferredAuthorization()) {
            if (!m_methodAttemptActive || auth->requestId() != m_requestId)
                return;
            if (m_methodAttemptActive && auth->requestId() == m_requestId) {
            if (auth == m_loserAuth) {
                m_loserAuth.clear();
                m_attemptGate.exited(auth->method());
                m_waitingForLoser = false;
                authorizeWinner();
                return;
            }
            if (auth != m_winnerAuth) {
                if (auth->method() == AuthMethod::Fingerprint)
                    m_socketServer->authenticationMethodState(m_attemptSocket, m_requestId, 5, QStringLiteral("failed"));
                else if (m_attemptSocket)
                    emit loginFailed(m_attemptSocket);
                return; // one method failed; keep the other method available
            }
            if (status != Auth::HELPER_SUCCESS) {
                if (m_attemptSocket)
                    emit loginFailed(m_attemptSocket);
                finishMethodAttempt(false);
                return;
            }
            if (!m_attemptReuseSessionId.isNull()) {
                OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
                auto unlocked = manager.UnlockSession(m_attemptReuseSessionId);
                unlocked.waitForFinished();
                if (unlocked.isError()) {
                    qWarning() << "Failed to unlock existing logind session:" << unlocked.error();
                    if (m_attemptSocket)
                        emit loginFailed(m_attemptSocket);
                    finishMethodAttempt(false);
                    return;
                }
                auto activated = manager.ActivateSession(m_attemptReuseSessionId);
                activated.waitForFinished();
                if (activated.isError()) {
                    qWarning() << "Failed to activate existing logind session:" << activated.error();
                    if (m_attemptSocket)
                        emit loginFailed(m_attemptSocket);
                    finishMethodAttempt(false);
                    return;
                }
                m_started = true;
                if (mainConfig.Users.RememberLastUser.get())
                    stateConfig.Last.User.set(m_attemptUser);
                else
                    stateConfig.Last.User.setDefault();
                if (mainConfig.Users.RememberLastSession.get())
                    stateConfig.Last.Session.set(m_attemptSession.fileName());
                else
                    stateConfig.Last.Session.setDefault();
                stateConfig.save();
                if (m_attemptSocket)
                    emit loginSucceeded(m_attemptSocket);
                finishMethodAttempt(true);
                // Reuse activates the existing session; retire this greeter's
                // display server so logout can create a fresh greeter.
                QTimer::singleShot(0, this, &Display::stop);
            }
            return;
            }
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
        if (!auth || auth->method() != AuthMethod::Password)
            return;
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
        Auth *auth = qobject_cast<Auth *>(sender());
        if (m_methodAttemptActive && auth == m_winnerAuth) {
            if (success) {
                m_sessionAuth = auth;
                if (mainConfig.Users.RememberLastUser.get())
                    stateConfig.Last.User.set(m_attemptUser);
                else
                    stateConfig.Last.User.setDefault();
                if (mainConfig.Users.RememberLastSession.get())
                    stateConfig.Last.Session.set(m_attemptSession.fileName());
                else
                    stateConfig.Last.Session.setDefault();
                stateConfig.save();
                if (m_attemptSocket)
                    emit loginSucceeded(m_attemptSocket);
                finishMethodAttempt(true);
            } else {
                if (m_attemptSocket)
                    emit loginFailed(m_attemptSocket);
                finishMethodAttempt(false);
            }
        }
        qDebug() << "Session started" << success;
        if (success)
            QTimer::singleShot(5000, m_greeter, &Greeter::stop);
    }
}
