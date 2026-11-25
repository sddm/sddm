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
#include "PowerManager.h"

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QUuid>
#include <QLocalSocket>

#include <pwd.h>
#include <unistd.h>
#include <sys/time.h>
#include <cstring>

#include <sys/ioctl.h>
#include <fcntl.h>

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusReply>

#include "Login1Manager.h"
#include "Login1Session.h"
#include "VirtualTerminal.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include "WaylandDisplayServer.h"
#include "config.h"

static int s_ttyFailures = 0;
#define STRINGIFY(x) #x

namespace {
    void wipeSecretString(QString &secret) {
        if (!secret.isEmpty()) {
            secret.detach();
            explicit_bzero(secret.data(), size_t(secret.size()) * sizeof(QChar));
            secret.clear();
        }
    }
}

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
        : Display(parent, serverType, parent && parent->canTTY()
                    ? (serverType == X11DisplayServerType
                        ? VirtualTerminal::setUpNewVt() : fetchAvailableVt())
                    : -1)
    {
    }

    Display::Display(Seat *parent, DisplayServerType serverType, int terminalId)
        : QObject(parent),
        m_displayServerType(serverType),
        m_terminalId(terminalId),
        m_auth(new Auth(this)),
        m_fingerprintAuth(new Auth(this)),
        m_seat(parent),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this))
    {
        m_fprintServiceWatcher = new QDBusServiceWatcher(
                QStringLiteral("net.reactivated.Fprint"), QDBusConnection::systemBus(),
                QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_fprintServiceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                [this](const QString &, const QString &oldOwner, const QString &newOwner) {
            // A bounded probe may be activating an unowned service. Accept
            // its initial acquisition, but never carry replies across loss
            // or replacement of an owner already bound to that probe.
            if (m_probeDeadline && oldOwner.isEmpty() && !newOwner.isEmpty()
                && (m_probeServiceOwner.isEmpty() || m_probeServiceOwner == newOwner))
                return;
            invalidateProbe();
            if (m_contextOptedIn && !m_pendingContext && !m_contextFinished)
                setFingerprintAvailability(m_contextSerial, m_requestId, false,
                                           QStringLiteral("service_unavailable"));
            // If fprintd vanishes mid-scan, stop the uncommitted fingerprint
            // helper immediately without touching any concurrent password attempt.
            if (newOwner.isEmpty() && !m_tearingDown && !m_contextCommitted && !m_methodWinnerChosen
                && m_fingerprintAuth->isActive() && isCurrentHelper(m_fingerprintAuth)
                && !m_cancelledHelpers.contains(m_fingerprintAuth)) {
                m_fingerprintStartRequested = false;
                m_pendingFingerprintRetryGeneration = 0;
                m_attemptGate.cancel(AuthMethod::Fingerprint);
                disarmFingerprintDeadline();
                m_fingerprintTerminal = true;
                m_fingerprintTerminalState = FingerprintState::Unavailable;
                m_fingerprintTerminalDetail = QStringLiteral("service_unavailable");
                stopAuthenticationHelper(m_fingerprintAuth);
            }
        });
        QDBusConnection::systemBus().connect(
                QStringLiteral("org.freedesktop.login1"),
                QStringLiteral("/org/freedesktop/login1"),
                QStringLiteral("org.freedesktop.login1.Manager"),
                QStringLiteral("PrepareForSleep"),
                this, SLOT(slotPrepareForSleep(bool)));
        // Create display server
        switch (m_displayServerType) {
        case X11DisplayServerType:
            // The terminal is resolved by the public constructor.
            m_displayServer = new XorgDisplayServer(this);
            break;
        case X11UserDisplayServerType:
            m_displayServer = new XorgUserDisplayServer(this);
            m_greeter->setDisplayServerCommand(XorgUserDisplayServer::command(this));
            break;
        case WaylandDisplayServerType:
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
        connect(m_fingerprintAuth, &Auth::authenticationMethodState,
                this, &Display::slotAuthenticationMethodState);
        connect(m_fingerprintAuth, &Auth::info, this, &Display::slotAuthInfo);
        connect(m_fingerprintAuth, &Auth::error, this, &Display::slotAuthError);

        // restart display after display server ended
        connect(m_displayServer, &DisplayServer::started, this, &Display::displayServerStarted);
        connect(m_displayServer, &DisplayServer::stopped, this, &Display::stop);

        connect(m_socketServer, &SocketServer::login, this, &Display::login);
        connect(m_socketServer, &SocketServer::setAuthenticationContext, this, &Display::setAuthenticationContext);
        connect(m_socketServer, &SocketServer::startFingerprint, this, &Display::startFingerprint);
        connect(m_socketServer, &SocketServer::cancelFingerprint, this, &Display::cancelFingerprint);
        connect(m_socketServer, &SocketServer::disconnected, this, &Display::retireSocket);
        connect(m_socketServer, &SocketServer::powerRequested, this, &Display::requestPower);
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
        if (((daemonApp && daemonApp->first) || mainConfig.Autologin.Relogin.get()) &&
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
        if (daemonApp)
            daemonApp->first = false;
    }

    Display::~Display() {
        // Destruction can run after the main-thread event dispatcher is gone.
        for (Auth *auth : {m_auth, m_fingerprintAuth})
            disconnect(auth, nullptr, this, nullptr);
        stopInternal(true);
        wipeSecretString(m_passPhrase);
        wipeSecretString(m_pendingPassword);
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
        m_tearingDown = true;
        m_pendingPower = -1;
        m_pendingContext = false;
        m_pendingFingerprintStart = false;
        m_fingerprintStartRequested = false;
        clearPendingLogin();
        invalidateProbe();
        cancelMethodAttempt(destroying);
        for (Auth *auth : {m_auth, m_fingerprintAuth})
            stopAuthenticationHelper(auth, destroying);
        if (!m_started)
            return;
        m_greeter->stop();
        m_socketServer->stop();
        m_displayServer->blockSignals(true);
        m_displayServer->stop();
        m_displayServer->blockSignals(false);
        m_started = false;
        emit stopped();
    }

    void Display::login(QLocalSocket *socket, const QString &user, const QString &password,
                        const Session &session) {
        if (!socket || m_tearingDown || m_contextCommitted)
            return;
        if (user.isEmpty() || user == QLatin1String("sddm") || !session.isValid()) {
            emit loginFailed(socket);
            return;
        }
        const auto owner = m_pendingContext ? m_pendingContextSocket : m_contextSocket;
        if ((owner && socket != owner) || (m_methodAttemptActive && socket != m_attemptSocket)) {
            emit loginFailed(socket);
            return;
        }
        if (m_pendingContext) {
            if (!m_pendingHasContext || user != m_pendingContextUser
                || session.type() != m_pendingContextSession.type()
                || session.fileName() != m_pendingContextSession.fileName())
                return;
            m_pendingLogin = true;
            m_pendingSocket = socket;
            m_pendingUser = user;
            m_pendingPassword = password;
            m_pendingSession = session;
            return;
        }
        if (m_contextOptedIn && (m_contextFinished || user != m_contextUser
            || session.type() != m_contextSession.type() || session.fileName() != m_contextSession.fileName()))
            return;
        if (m_methodAttemptActive && m_methodWinnerChosen)
            return;
        if ((m_auth->isActive() || m_fingerprintAuth->isActive())
            && (!m_methodAttemptActive || user != m_attemptUser
                || session.type() != m_attemptSession.type() || session.fileName() != m_attemptSession.fileName())) {
            m_pendingLogin = true;
            m_pendingPasswordGeneration = 0;
            m_pendingSocket = socket;
            m_pendingUser = user;
            m_pendingPassword = password;
            m_pendingSession = session;
            cancelMethodAttempt();
            startPendingLogin();
            return;
        }
        if (m_auth->isActive()) {
            // Only an explicit submission after this generation's terminal
            // decision is a retry. A live nonterminal duplicate is idempotent.
            if (isCurrentHelper(m_auth) && m_attemptGate.cancelled(AuthMethod::Password)
                && !m_cancelledHelpers.contains(m_auth)) {
                m_pendingLogin = true;
                m_pendingSocket = socket;
                m_pendingUser = user;
                m_pendingPassword = password;
                m_pendingSession = session;
                m_pendingPasswordGeneration = m_passwordGeneration;
            }
            return;
        }
        if (!m_methodAttemptActive)
            beginMethodAttempt(socket, user, session);
        m_passwordSnapshot.valid = false;
        if (!startAuth(user, password, session, AuthMethod::Password)) {
            publishMethodState(AuthenticationMethod::Password, FingerprintState::Failed,
                               AuthenticationPhase::Active, QStringLiteral("authentication_failed"));
            emit loginFailed(socket);
        }
    }

    void Display::setAuthenticationContext(QLocalSocket *socket, quint64 serial, bool hasContext,
                                           const QString &user, const Session &session) {
        if (!socket || m_tearingDown)
            return;
        const auto owner = m_pendingContext ? m_pendingContextSocket : m_contextSocket;
        if ((owner && owner != socket) || (m_methodAttemptActive && m_attemptSocket != socket)
            || (m_contextCommitted && m_contextSocket != socket)) {
            m_socketServer->authenticationContext(socket, serial, 0, QString(), QString(), Session(),
                    quint32(AuthenticationAvailability::Unavailable), quint32(AuthenticationPhase::Idle),
                    QStringLiteral("context_owned"));
            return;
        }
        const quint64 previous = m_selectionSerials.value(socket);
        if (!serial || serial <= previous || (hasContext && (user.isEmpty()
            || user == QLatin1String("sddm") || !session.isValid()))) {
            m_socketServer->authenticationContext(socket, serial, 0, QString(), QString(), Session(),
                    quint32(AuthenticationAvailability::Unavailable), quint32(AuthenticationPhase::Idle),
                    QStringLiteral("invalid_context"));
            return;
        }
        m_selectionSerials.insert(socket, serial);
        if (m_contextCommitted) {
            m_socketServer->authenticationContext(socket, serial, 0, m_requestId, m_contextUser,
                    m_contextSession, quint32(m_fingerprintAvailability), quint32(m_contextPhase),
                    QStringLiteral("session_committed"));
            return;
        }
        invalidateProbe();
        clearPendingLogin();
        m_pendingContext = true;
        m_pendingContextSocket = socket;
        m_pendingContextSerial = serial;
        m_pendingHasContext = hasContext;
        m_pendingContextUser = hasContext ? user : QString();
        m_pendingContextSession = hasContext ? session : Session();
        m_pendingFingerprintStart = false;
        m_pendingFingerprintCancelled = false;
        m_fingerprintStartRequested = false;
        cancelMethodAttempt();
        advancePendingContext();
    }
    void Display::setFingerprintAvailability(quint64 serial, const QString &requestId,
                                            bool available, const QString &detailCode) {
        if (serial != m_contextSerial || requestId != m_requestId || !m_contextOptedIn
            || !m_contextSocket || m_contextCommitted || m_contextFinished || m_pendingContext || m_tearingDown)
            return;
        invalidateProbe();
        m_fingerprintAvailability = available ? AuthenticationAvailability::Available
                                             : AuthenticationAvailability::Unavailable;
        m_socketServer->authenticationContext(m_contextSocket, serial, 1, requestId, m_contextUser,
                m_contextSession, quint32(m_fingerprintAvailability), quint32(m_contextPhase), detailCode);
        if (!m_fingerprintAuth->isActive() && !m_fingerprintTerminal)
            publishMethodState(AuthenticationMethod::Fingerprint,
                    available ? FingerprintState::Ready : FingerprintState::Unavailable, m_contextPhase,
                    available ? QStringLiteral("available") : detailCode);
        if (m_fingerprintStartRequested) {
            if (available)
                startFingerprint(m_contextSocket, serial);
            else
                m_fingerprintStartRequested = false;
        }
    }

    void Display::probeFingerprintAvailability() {
        if (!m_contextOptedIn || !m_contextSocket || m_pendingContext || m_contextFinished
            || m_contextCommitted || m_tearingDown)
            return;
        invalidateProbe();
        const quint64 generation = m_probeGeneration;
        const quint64 serial = m_contextSerial;
        const QString requestId = m_requestId;
        m_fingerprintAvailability = AuthenticationAvailability::Unknown;
        m_socketServer->authenticationContext(m_contextSocket, serial, 1, requestId, m_contextUser, m_contextSession,
                quint32(AuthenticationAvailability::Unknown), quint32(m_contextPhase), QStringLiteral("checking"));
        auto *deadline = new QTimer(this);
        deadline->setSingleShot(true);
        m_probeDeadline = deadline;
        connect(deadline, &QTimer::timeout, this, [this, generation, serial, requestId] {
            if (generation == m_probeGeneration)
                setFingerprintAvailability(serial, requestId, false, QStringLiteral("availability_unavailable"));
        });
        deadline->start(5000);
        resolveFingerprintServiceOwner(generation, true);
    }

    void Display::resolveFingerprintServiceOwner(quint64 generation, bool allowActivation) {
        if (generation != m_probeGeneration)
            return;
        const quint64 serial = m_contextSerial;
        const QString requestId = m_requestId;
        const QPointer<QLocalSocket> socket = m_contextSocket;
        const QString user = m_contextUser;
        const Session session = m_contextSession;
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("GetNameOwner"));
        call << QStringLiteral("net.reactivated.Fprint");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, 2000), this);
        m_probeWatchers.insert(watcher);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, generation, serial, requestId, socket, user, session, allowActivation] {
            QDBusPendingReply<QString> ownerReply = *watcher;
            m_probeWatchers.remove(watcher);
            watcher->deleteLater();
            if (generation != m_probeGeneration || socket != m_contextSocket
                || serial != m_contextSerial || requestId != m_requestId || user != m_contextUser
                || session.type() != m_contextSession.type() || session.fileName() != m_contextSession.fileName())
                return;
            if (allowActivation && ownerReply.isError()
                && ownerReply.error().name() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")) {
                // One activation per probe; retries are user intent, not a
                // service-liveness loop. The original five-second deadline
                // also bounds activation and all subsequent read-only calls.
                auto activate = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("StartServiceByName"));
                activate << QStringLiteral("net.reactivated.Fprint") << quint32(0);
                // The overall probe deadline owns the budget; a second 5s timer races its terminal reason.
                auto *activationWatcher = new QDBusPendingCallWatcher(
                        QDBusConnection::systemBus().asyncCall(activate), this);
                m_probeWatchers.insert(activationWatcher);
                connect(activationWatcher, &QDBusPendingCallWatcher::finished, this,
                        [this, activationWatcher, generation, serial, requestId] {
                    QDBusPendingReply<quint32> activation = *activationWatcher;
                    m_probeWatchers.remove(activationWatcher);
                    activationWatcher->deleteLater();
                    if (generation != m_probeGeneration)
                        return;
                    if (activation.isError() || (activation.value() != 1 && activation.value() != 2)) {
                        setFingerprintAvailability(serial, requestId, false,
                                                   QStringLiteral("service_unavailable"));
                        return;
                    }
                    resolveFingerprintServiceOwner(generation, false);
                });
                return;
            }
            if (ownerReply.isError() || ownerReply.value().isEmpty()) {
                setFingerprintAvailability(serial, requestId, false, QStringLiteral("service_unavailable"));
                return;
            }
            const QString owner = ownerReply.value();
            m_probeServiceOwner = owner;
            auto devicesCall = QDBusMessage::createMethodCall(owner,
                    QStringLiteral("/net/reactivated/Fprint/Manager"),
                    QStringLiteral("net.reactivated.Fprint.Manager"), QStringLiteral("GetDevices"));
            auto *devicesWatcher = new QDBusPendingCallWatcher(
                    QDBusConnection::systemBus().asyncCall(devicesCall, 2000), this);
            m_probeWatchers.insert(devicesWatcher);
            connect(devicesWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, devicesWatcher, owner, generation, serial, requestId] {
                QDBusPendingReply<QList<QDBusObjectPath>> reply = *devicesWatcher;
                m_probeWatchers.remove(devicesWatcher);
                devicesWatcher->deleteLater();
                if (generation != m_probeGeneration)
                    return;
                if (reply.isError()) {
                    setFingerprintAvailability(serial, requestId, false, QStringLiteral("service_unavailable"));
                    return;
                }
                if (reply.value().isEmpty()) {
                    setFingerprintAvailability(serial, requestId, false, QStringLiteral("reader_unavailable"));
                    return;
                }
                probeEnrolledDevices(reply.value().mid(0, 8), 0, reply.value().size() > 8, owner, generation);
            });
        });
    }

    void Display::startFingerprint(QLocalSocket *socket, quint64 serial) {
        if (m_tearingDown || m_contextCommitted)
            return;
        if (m_pendingContext) {
            if (socket == m_pendingContextSocket && serial == m_pendingContextSerial && m_pendingHasContext) {
                m_pendingFingerprintStart = true;
                m_pendingFingerprintCancelled = false;
            }
            return;
        }
        if (socket != m_contextSocket || serial != m_contextSerial || !m_contextOptedIn
            || m_contextFinished || m_methodWinnerChosen)
            return;
        if (m_fingerprintAuth->isActive()) {
            if (isCurrentHelper(m_fingerprintAuth) && m_fingerprintTerminal)
                m_pendingFingerprintRetryGeneration = m_fingerprintGeneration;
            return;
        }
        m_fingerprintStartRequested = true;
        if (m_fingerprintTerminal) {
            // Explicit terminal retries re-probe only after observed exit;
            // never wait for or stop a concurrent password transaction.
            m_fingerprintTerminal = false;
            m_fingerprintAvailability = AuthenticationAvailability::Unknown;
            probeFingerprintAvailability();
            return;
        }
        if (m_fingerprintAvailability != AuthenticationAvailability::Available) {
            if (m_fingerprintAvailability != AuthenticationAvailability::Unknown)
                probeFingerprintAvailability();
            return;
        }
        m_fingerprintStartRequested = false;
        if (!m_methodAttemptActive)
            beginMethodAttempt(socket, m_contextUser, m_contextSession);
        m_fingerprintTerminal = false;
        m_fingerprintSnapshot.valid = false;
        if (!startAuth(m_contextUser, QString(), m_contextSession, AuthMethod::Fingerprint)) {
            m_fingerprintTerminal = true;
            m_fingerprintTerminalState = FingerprintState::Unavailable;
            m_fingerprintTerminalDetail = QStringLiteral("authentication_unavailable");
            publishMethodState(AuthenticationMethod::Fingerprint, m_fingerprintTerminalState,
                               AuthenticationPhase::Active, m_fingerprintTerminalDetail);
        }
        // Only helper METHOD_STATE says PAM authentication has actually begun.
    }

    void Display::cancelFingerprint(QLocalSocket *socket, quint64 serial, const QString &requestId) {
        if (m_tearingDown || m_contextCommitted)
            return;
        if (m_pendingContext) {
            if (socket == m_pendingContextSocket && serial == m_pendingContextSerial
                && m_pendingHasContext && requestId.isEmpty()) {
                m_pendingFingerprintStart = false;
                m_pendingFingerprintCancelled = true;
            }
            return;
        }
        if (socket != m_contextSocket || serial != m_contextSerial || !m_contextOptedIn
            || m_contextFinished || m_methodWinnerChosen || (!requestId.isEmpty() && requestId != m_requestId))
            return;
        m_fingerprintStartRequested = false;
        m_pendingFingerprintRetryGeneration = 0;
        if (!m_fingerprintAuth->isActive()) {
            m_fingerprintTerminal = true;
            m_fingerprintTerminalState = FingerprintState::Cancelled;
            m_fingerprintTerminalDetail = QStringLiteral("cancelled");
            publishMethodState(AuthenticationMethod::Fingerprint, FingerprintState::Cancelled,
                               m_contextPhase, QStringLiteral("cancelled"));
            return;
        }
        if (!isCurrentHelper(m_fingerprintAuth) || m_cancelledHelpers.contains(m_fingerprintAuth))
            return;
        m_attemptGate.cancel(AuthMethod::Fingerprint);
        disarmFingerprintDeadline();
        if (!m_fingerprintTerminal) {
            m_fingerprintTerminal = true;
            m_fingerprintTerminalState = FingerprintState::Cancelled;
            m_fingerprintTerminalDetail = QStringLiteral("cancelled");
        }
        stopAuthenticationHelper(m_fingerprintAuth);
    }

    void Display::cancelMethodAttempt(bool destroying) {
        m_pendingFingerprintRetryGeneration = 0;
        m_fingerprintStartRequested = false;
        if (m_pendingPasswordGeneration)
            clearPendingLogin();
        disarmFingerprintDeadline();
        m_attemptGate.invalidate();
        m_methodAttemptActive = false;
        m_methodWinnerChosen = false;
        m_waitingForLoser = false;
        for (Auth *auth : {m_auth, m_fingerprintAuth}) {
            if (auth != m_sessionAuth && auth->isActive())
                stopAuthenticationHelper(auth, destroying);
        }
        m_winnerAuth.clear();
        m_loserAuth.clear();
        m_attemptReuseSessionId.clear();
        m_reuseSessionSelected = false;
        m_attemptSocket.clear();
        m_socket.clear();
        wipeSecretString(m_passPhrase);
    }

    void Display::authorizeWinner() {
        if (m_tearingDown || m_pendingContext || !m_methodAttemptActive || !m_winnerAuth
            || !isCurrentHelper(m_winnerAuth) || !m_winnerAuth->isActive()
            || !m_winnerAuth->isAwaitingAuthorization() || m_waitingForLoser
            || !m_attemptGate.authorize(m_winnerAuth->method()))
            return;
        if (auto *server = qobject_cast<XorgDisplayServer *>(m_displayServer))
            m_winnerAuth->setCookie(server->cookie());
        m_contextCommitted = true;
        m_contextPhase = AuthenticationPhase::SessionAuthorized;
        publishMethodState(AuthenticationMethod(m_winnerAuth->method()), FingerprintState::Matched,
                           m_contextPhase, QStringLiteral("matched"));
        m_winnerAuth->authorizeSession(true);
    }

    void Display::startPendingLogin() {
        if (!m_pendingLogin || m_pendingContext || m_tearingDown || m_contextCommitted
            || m_methodWinnerChosen || m_auth->isActive())
            return;
        if (m_pendingPasswordGeneration) {
            if (!m_methodAttemptActive || m_contextFinished
                || m_passwordGeneration != m_pendingPasswordGeneration
                || m_pendingSocket != m_attemptSocket || m_pendingUser != m_attemptUser
                || m_pendingSession.type() != m_attemptSession.type()
                || m_pendingSession.fileName() != m_attemptSession.fileName()) {
                clearPendingLogin();
                return;
            }
        } else if (m_fingerprintAuth->isActive()) {
            return;
        }
        const QPointer<QLocalSocket> socket = m_pendingSocket;
        const QString user = m_pendingUser;
        QString password = m_pendingPassword;
        const Session session = m_pendingSession;
        clearPendingLogin();
        if (socket && socket->state() == QLocalSocket::ConnectedState)
            login(socket, user, password, session);
        wipeSecretString(password);
    }

    void Display::finishMethodAttempt(bool success) {
        if (!m_methodAttemptActive)
            return;
        disarmFingerprintDeadline();
        invalidateProbe();
        m_pendingFingerprintRetryGeneration = 0;
        m_fingerprintStartRequested = false;
        clearPendingLogin();
        const auto socket = m_attemptSocket;
        const quint32 method = m_winnerAuth ? quint32(m_winnerAuth->method()) : quint32(AuthenticationMethod::NoWinner);
        m_contextPhase = AuthenticationPhase::Finished;
        if (socket && m_contextOptedIn)
            m_socketServer->authenticationAttemptFinished(socket, m_contextSerial, m_requestId, m_attemptUser,
                    m_attemptSession, quint32(success ? AuthenticationResult::Succeeded : AuthenticationResult::Failed), method);
        m_methodAttemptActive = false;
        m_methodWinnerChosen = false;
        m_attemptGate.invalidate();
        m_waitingForLoser = false;
        m_winnerAuth.clear();
        m_loserAuth.clear();
        m_attemptSocket.clear();
        m_attemptReuseSessionId.clear();
        m_reuseSessionSelected = false;
        m_contextFinished = m_contextOptedIn;
        m_contextCommitted = success;
        m_socket.clear();
        wipeSecretString(m_passPhrase);
    }
    void Display::publishMethodState(AuthenticationMethod method, FingerprintState state,
                                     AuthenticationPhase phase, const QString &detailCode) {
        if (!m_contextOptedIn || !m_contextSocket || m_pendingContext || m_tearingDown)
            return;
        auto &snapshot = method == AuthenticationMethod::Fingerprint ? m_fingerprintSnapshot : m_passwordSnapshot;
        if (snapshot.valid && snapshot.state == state && snapshot.phase == phase && snapshot.detail == detailCode)
            return;
        snapshot = {true, state, phase, detailCode};
        m_socketServer->authenticationMethodState(m_contextSocket, m_contextSerial, m_requestId,
                quint32(method), m_contextUser, m_contextSession, quint32(state), quint32(phase), detailCode);
    }

    void Display::invalidateProbe() {
        ++m_probeGeneration;
        m_probeServiceOwner.clear();
        if (m_probeDeadline) {
            m_probeDeadline->stop();
            m_probeDeadline->deleteLater();
            m_probeDeadline.clear();
        }
        for (auto *watcher : qAsConst(m_probeWatchers)) {
            disconnect(watcher, nullptr, this, nullptr);
            watcher->deleteLater();
        }
        m_probeWatchers.clear();
    }

    void Display::probeEnrolledDevices(const QList<QDBusObjectPath> &devices, int index, bool hadError,
                                       const QString &owner, quint64 generation) {
        if (generation != m_probeGeneration)
            return;
        if (index == devices.size()) {
            setFingerprintAvailability(m_contextSerial, m_requestId, false,
                    hadError ? QStringLiteral("availability_unavailable") : QStringLiteral("not_enrolled"));
            return;
        }
        auto call = QDBusMessage::createMethodCall(owner, devices.at(index).path(),
                QStringLiteral("net.reactivated.Fprint.Device"), QStringLiteral("ListEnrolledFingers"));
        call << m_contextUser;
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, 2000), this);
        m_probeWatchers.insert(watcher);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, devices, index, hadError, owner, generation] {
            QDBusPendingReply<QStringList> reply = *watcher;
            m_probeWatchers.remove(watcher);
            watcher->deleteLater();
            if (generation != m_probeGeneration)
                return;
            if (!reply.isError() && !reply.value().isEmpty()) {
                confirmFingerprintAvailability(owner, generation);
                return;
            }
            probeEnrolledDevices(devices, index + 1, hadError || reply.isError(), owner, generation);
        });
    }

    void Display::confirmFingerprintAvailability(const QString &owner, quint64 generation) {
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
                QStringLiteral("GetNameOwner"));
        call << QStringLiteral("net.reactivated.Fprint");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, 2000), this);
        m_probeWatchers.insert(watcher);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, owner, generation] {
            QDBusPendingReply<QString> reply = *watcher;
            m_probeWatchers.remove(watcher);
            watcher->deleteLater();
            if (generation != m_probeGeneration)
                return;
            const bool currentOwner = !reply.isError() && reply.value() == owner;
            setFingerprintAvailability(m_contextSerial, m_requestId, currentOwner,
                    currentOwner ? QStringLiteral("available") : QStringLiteral("service_unavailable"));
        });
    }

    void Display::clearPendingLogin() {
        m_pendingLogin = false;
        m_pendingPasswordGeneration = 0;
        m_pendingSocket.clear();
        m_pendingUser.clear();
        wipeSecretString(m_pendingPassword);
        m_pendingSession = Session();
    }

    void Display::beginMethodAttempt(QLocalSocket *socket, const QString &user, const Session &session) {
        m_socket = socket;
        m_attemptSocket = socket;
        m_attemptUser = user;
        m_attemptSession = session;
        if (!m_contextOptedIn) {
            m_requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_contextSocket = socket;
            m_contextUser = user;
            m_contextSession = session;
        }
        m_methodAttemptActive = true;
        m_methodWinnerChosen = false;
        m_waitingForLoser = false;
        m_contextPhase = AuthenticationPhase::Active;
        m_attemptGate.begin(m_requestId, user);
    }

    void Display::advancePendingContext() {
        if (!m_pendingContext || m_tearingDown || m_auth->isActive() || m_fingerprintAuth->isActive())
            return;
        const bool start = m_pendingFingerprintStart;
        const bool cancelled = m_pendingFingerprintCancelled;
        m_contextSocket = m_pendingContextSocket;
        m_contextSerial = m_pendingContextSerial;
        m_contextOptedIn = m_pendingHasContext;
        m_contextUser = m_pendingContextUser;
        m_contextSession = m_pendingContextSession;
        m_requestId = m_contextOptedIn ? QUuid::createUuid().toString(QUuid::WithoutBraces) : QString();
        m_contextCommitted = false;
        m_contextFinished = false;
        m_contextPhase = AuthenticationPhase::Idle;
        m_fingerprintAvailability = m_contextOptedIn ? AuthenticationAvailability::Unknown
                                                     : AuthenticationAvailability::Unavailable;
        m_fingerprintTerminal = cancelled;
        m_fingerprintTerminalState = cancelled ? FingerprintState::Cancelled : FingerprintState::Failed;
        m_fingerprintTerminalDetail = cancelled ? QStringLiteral("cancelled") : QString();
        m_passwordSnapshot.valid = false;
        m_fingerprintSnapshot.valid = false;
        m_pendingContext = false;
        m_pendingContextSocket.clear();
        m_pendingContextUser.clear();
        m_pendingContextSession = Session();
        m_pendingFingerprintStart = false;
        m_pendingFingerprintCancelled = false;
        if (!m_contextSocket)
            return;
        m_socketServer->authenticationContext(m_contextSocket, m_contextSerial, 1, m_requestId,
                m_contextUser, m_contextSession, quint32(m_fingerprintAvailability),
                quint32(AuthenticationPhase::Idle), m_contextOptedIn ? QStringLiteral("checking") : QStringLiteral("cleared"));
        if (!m_contextOptedIn) {
            m_contextSocket.clear();
            return;
        }
        if (cancelled)
            publishMethodState(AuthenticationMethod::Fingerprint, FingerprintState::Cancelled,
                               AuthenticationPhase::Idle, QStringLiteral("cancelled"));
        m_fingerprintStartRequested = start;
        probeFingerprintAvailability();
        startPendingLogin();
    }

    bool Display::isCurrentHelper(Auth *auth) const {
        if (!auth || !m_methodAttemptActive || auth->requestId() != m_requestId)
            return false;
        return auth == m_auth ? auth->launchGeneration() == m_passwordGeneration
                             : auth == m_fingerprintAuth && auth->launchGeneration() == m_fingerprintGeneration;
    }

    void Display::stopAuthenticationHelper(Auth *auth, bool destroying) {
        if (!auth || !auth->isActive())
            return;
        if (m_cancelledHelpers.contains(auth)) {
            if (destroying)
                auth->stop();
            return;
        }
        m_cancelledHelpers.insert(auth);
        auth->authorizeSession(false);
        if (destroying) {
            auth->stop();
            return;
        }
        auth->terminate();
        const QPointer<Auth> guarded(auth);
        const quint64 generation = auth->launchGeneration();
        QTimer::singleShot(5000, auth, [guarded, generation] {
            if (guarded && guarded->launchGeneration() == generation && guarded->isActive())
                guarded->kill();
        });
    }

    void Display::disarmFingerprintDeadline() {
        m_fingerprintAwaitingAuthentication = false;
        if (m_fingerprintDeadline) {
            m_fingerprintDeadline->stop();
            m_fingerprintDeadline->deleteLater();
            m_fingerprintDeadline.clear();
        }
    }

    void Display::armFingerprintDeadline(Auth *auth) {
        disarmFingerprintDeadline();
        m_fingerprintGeneration = auth->launchGeneration();
        m_fingerprintAwaitingAuthentication = true;
        const quint64 generation = m_fingerprintGeneration;
        const QString requestId = m_requestId;
        const quint64 serial = m_contextSerial;
        const QPointer<QLocalSocket> socket = m_contextSocket;
        auto *deadline = new QTimer(this);
        deadline->setSingleShot(true);
        m_fingerprintDeadline = deadline;
        connect(deadline, &QTimer::timeout, this, [this, auth, generation, requestId, serial, socket] {
            if (!m_fingerprintAwaitingAuthentication || !isCurrentHelper(auth) || m_methodWinnerChosen
                || m_requestId != requestId || m_contextSerial != serial || m_contextSocket != socket
                || auth->launchGeneration() != generation || m_pendingContext || m_tearingDown)
                return;
            disarmFingerprintDeadline();
            m_attemptGate.cancel(AuthMethod::Fingerprint);
            m_fingerprintTerminal = true;
            m_fingerprintTerminalState = FingerprintState::TimedOut;
            m_fingerprintTerminalDetail = QStringLiteral("authentication_timeout");
            stopAuthenticationHelper(auth);
        });
        // Bound invalid/overflowing configuration without an unbounded deadline.
        deadline->start(qBound(1, mainConfig.FingerprintAuthTimeout.get(), 3600) * 1000);
    }

    void Display::retireSocket(QLocalSocket *socket) {
        m_selectionSerials.remove(socket);
        if (m_pendingContextSocket == socket) {
            m_pendingContext = false;
            m_pendingContextSocket.clear();
        }
        if (m_pendingSocket == socket)
            clearPendingLogin();
        if (m_contextSocket == socket || m_attemptSocket == socket) {
            invalidateProbe();
            m_fingerprintStartRequested = false;
            if (!m_contextCommitted) {
                cancelMethodAttempt();
                m_contextOptedIn = false;
                m_contextSocket.clear();
            }
        }
    }

    void Display::requestPower(GreeterMessages action) {
        if (m_pendingPower != -1)
            return;
        m_pendingPower = int(action);
        m_tearingDown = true;
        m_pendingContext = false;
        m_fingerprintStartRequested = false;
        clearPendingLogin();
        invalidateProbe();
        cancelMethodAttempt();
        performPendingPower();
    }

    void Display::performPendingPower() {
        if (m_pendingPower == -1)
            return;
        for (Auth *auth : {m_auth, m_fingerprintAuth}) {
            if (auth != m_sessionAuth && auth->isActive())
                return;
        }
        const auto action = GreeterMessages(m_pendingPower);
        m_pendingPower = -1;
        QTimer::singleShot(0, this, [this, action] {
            auto *power = daemonApp ? daemonApp->powerManager() : nullptr;
            if (power) {
                switch (action) {
                case GreeterMessages::PowerOff: power->powerOff(); break;
                case GreeterMessages::Reboot: power->reboot(); break;
                case GreeterMessages::Suspend: power->suspend(); break;
                case GreeterMessages::Hibernate: power->hibernate(); break;
                case GreeterMessages::HybridSleep: power->hybridSleep(); break;
                default: Q_UNREACHABLE();
                }
            }
            // Resume needs a fresh visible context, never a hidden restart.
            m_tearingDown = false;
            m_contextOptedIn = false;
            m_contextSocket.clear();
            m_contextCommitted = bool(m_sessionAuth);
        });
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

        if (method == AuthMethod::Password) {
            wipeSecretString(m_passPhrase);
            m_passPhrase = password;
        }
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
        if (method == AuthMethod::Fingerprint)
            m_fingerprintGeneration = auth->launchGeneration() + 1;
        else
            m_passwordGeneration = auth->launchGeneration() + 1;
        auth->start();
        if (method == AuthMethod::Fingerprint && auth->isActive() && !m_fingerprintTerminal)
            armFingerprintDeadline(auth);

        return true;
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (auth && (m_tearingDown || m_pendingContext || m_cancelledHelpers.contains(auth)
            || (auth->isDeferredAuthorization() && m_attemptGate.cancelled(auth->method())))) {
            auth->authorizeSession(false);
            return;
        }
        if (isCurrentHelper(auth)) {
            if (success && m_methodWinnerChosen && auth == m_winnerAuth && user == m_attemptUser)
                return;
            if (!success || !auth->isActive() || !auth->isAwaitingAuthorization()) {
                m_attemptGate.cancel(auth->method());
                if (auth->method() == AuthMethod::Fingerprint) {
                    disarmFingerprintDeadline();
                    if (!m_fingerprintTerminal) {
                        m_fingerprintTerminal = true;
                        m_fingerprintTerminalState = FingerprintState::Failed;
                        m_fingerprintTerminalDetail = QStringLiteral("authentication_failed");
                    }
                    publishMethodState(AuthenticationMethod::Fingerprint, m_fingerprintTerminalState,
                                       m_contextPhase, m_fingerprintTerminalDetail);
                } else {
                    publishMethodState(AuthenticationMethod::Password, FingerprintState::Failed,
                                       m_contextPhase, QStringLiteral("authentication_failed"));
                    if (!m_contextOptedIn)
                        emit loginFailed(m_attemptSocket);
                }
                return;
            }
            if (auth == m_fingerprintAuth && m_fingerprintTerminal) {
                auth->authorizeSession(false);
                return;
            }
            if (!m_attemptGate.select(auth->requestId(), user, auth->method())) {
                stopAuthenticationHelper(auth);
                return;
            }
            disarmFingerprintDeadline();
            invalidateProbe();
            m_methodWinnerChosen = true;
            m_pendingFingerprintRetryGeneration = 0;
            m_fingerprintStartRequested = false;
            clearPendingLogin();
            m_winnerAuth = auth;
            m_contextPhase = AuthenticationPhase::WinnerSelected;
            publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Matched,
                               m_contextPhase, QStringLiteral("matched"));
            Auth *other = auth == m_auth ? m_fingerprintAuth : m_auth;
            if (other->isActive() && isCurrentHelper(other)) {
                m_loserAuth = other;
                m_attemptGate.waitFor(other->method());
                m_attemptGate.cancel(other->method());
                m_waitingForLoser = true;
                m_contextPhase = AuthenticationPhase::LoserStopping;
                publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Matched,
                                   m_contextPhase, QStringLiteral("matched"));
                if (other == m_fingerprintAuth && !m_fingerprintTerminal) {
                    m_fingerprintTerminal = true;
                    m_fingerprintTerminalState = FingerprintState::Cancelled;
                    m_fingerprintTerminalDetail = QStringLiteral("cancelled");
                }
                stopAuthenticationHelper(other);
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

    void Display::slotAuthenticationMethodState(const QString &requestId, AuthMethod method,
                                                FingerprintState state, const QString &detailCode) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!isCurrentHelper(auth) || auth != m_fingerprintAuth || method != AuthMethod::Fingerprint
            || requestId != m_requestId || m_pendingContext || m_tearingDown
            || m_cancelledHelpers.contains(auth) || m_methodWinnerChosen || m_fingerprintTerminal)
            return;
        if (state != FingerprintState::Scanning && state != FingerprintState::Retry
            && state != FingerprintState::Failed && state != FingerprintState::Unavailable)
            return;
        if (state == FingerprintState::Failed || state == FingerprintState::Unavailable) {
            disarmFingerprintDeadline();
            m_attemptGate.cancel(AuthMethod::Fingerprint);
            m_fingerprintTerminal = true;
            m_fingerprintTerminalState = state;
            m_fingerprintTerminalDetail = detailCode;
        }
        publishMethodState(AuthenticationMethod::Fingerprint, state, m_contextPhase, detailCode);
    }
    void Display::slotAuthInfo(const QString &message, Auth::Info info) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth || m_pendingContext || m_tearingDown || m_cancelledHelpers.contains(auth)
            || (auth->isDeferredAuthorization() && (!isCurrentHelper(auth) || m_methodWinnerChosen
                || m_attemptGate.cancelled(auth->method()))))
            return;
        if (m_contextOptedIn) {
            if (auth == m_auth)
                publishMethodState(AuthenticationMethod::Password, FingerprintState::Scanning,
                                   m_contextPhase, QStringLiteral("authentication_in_progress"));
            return;
        }
        QLocalSocket *socket = m_methodAttemptActive ? m_attemptSocket.data() : m_socket.data();
        if (socket) {
            qWarning() << "Authentication information:" << info << message;
            m_socketServer->informationMessage(socket, message);
        }
    }

    void Display::slotAuthError(const QString &message, Auth::Error error) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth || m_pendingContext || m_tearingDown || m_cancelledHelpers.contains(auth)
            || (auth->isDeferredAuthorization() && !isCurrentHelper(auth)))
            return;
        QLocalSocket *socket = m_methodAttemptActive ? m_attemptSocket.data() : m_socket.data();
        if (isCurrentHelper(auth) && auth != m_winnerAuth
            && (m_contextOptedIn || error == Auth::ERROR_AUTHENTICATION))
            m_attemptGate.cancel(auth->method());
        if (m_contextOptedIn) {
            if (auth == m_fingerprintAuth && m_fingerprintTerminal && auth != m_winnerAuth)
                return;
            const QString detail = error == Auth::ERROR_AUTHENTICATION ? QStringLiteral("authentication_failed")
                                                                       : QStringLiteral("account_or_session_failed");
            if (auth == m_fingerprintAuth) {
                disarmFingerprintDeadline();
                m_fingerprintTerminal = true;
                m_fingerprintTerminalState = FingerprintState::Failed;
                m_fingerprintTerminalDetail = detail;
            }
            publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Failed, m_contextPhase, detail);
            return;
        }
        if (!socket)
            return;
        qWarning() << "Authentication error:" << error << message;
        m_socketServer->informationMessage(socket, message);
        if (error == Auth::ERROR_AUTHENTICATION && auth != m_winnerAuth)
            emit loginFailed(socket);
    }

    void Display::slotHelperFinished(Auth::HelperExitStatus status) {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth)
            return;
        const bool stoppedHelper = m_cancelledHelpers.remove(auth);
        if (auth == m_sessionAuth) {
            m_sessionAuth.clear();
            if (m_pendingPower != -1)
                performPendingPower();
            else
                stop();
            return;
        }
        if (isCurrentHelper(auth)) {
            if (auth == m_fingerprintAuth) {
                disarmFingerprintDeadline();
                if (auth != m_winnerAuth) {
                    if (!m_fingerprintTerminal) {
                        m_fingerprintTerminal = true;
                        m_fingerprintTerminalState = stoppedHelper ? FingerprintState::Cancelled : FingerprintState::Failed;
                        m_fingerprintTerminalDetail = stoppedHelper ? QStringLiteral("cancelled")
                                                                    : QStringLiteral("authentication_failed");
                    }
                    publishMethodState(AuthenticationMethod::Fingerprint, m_fingerprintTerminalState,
                                       m_contextPhase, m_fingerprintTerminalDetail);
                }
            }
            if (auth == m_loserAuth) {
                m_loserAuth.clear();
                m_attemptGate.exited(auth->method());
                m_waitingForLoser = false;
                authorizeWinner();
            } else if (auth != m_winnerAuth) {
                if (auth == m_auth) {
                    publishMethodState(AuthenticationMethod::Password,
                            stoppedHelper ? FingerprintState::Cancelled : FingerprintState::Failed,
                            m_contextPhase, stoppedHelper ? QStringLiteral("cancelled") : QStringLiteral("authentication_failed"));
                    if (!m_contextOptedIn && !stoppedHelper)
                        emit loginFailed(m_attemptSocket);
                }
            } else if (status != Auth::HELPER_SUCCESS || !m_contextCommitted
                       || m_contextPhase != AuthenticationPhase::SessionAuthorized || m_attemptReuseSessionId.isNull()) {
                publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Failed,
                                   m_contextPhase, QStringLiteral("account_or_session_failed"));
                if (m_attemptSocket)
                    emit loginFailed(m_attemptSocket);
                finishMethodAttempt(false);
            } else {
                OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
                auto unlocked = manager.UnlockSession(m_attemptReuseSessionId);
                unlocked.waitForFinished();
                bool reused = false;
                if (!unlocked.isError()) {
                    auto activated = manager.ActivateSession(m_attemptReuseSessionId);
                    activated.waitForFinished();
                    reused = !activated.isError();
                }
                if (!reused) {
                    publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Failed,
                                       m_contextPhase, QStringLiteral("account_or_session_failed"));
                    if (m_attemptSocket)
                        emit loginFailed(m_attemptSocket);
                    finishMethodAttempt(false);
                } else {
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
                    QTimer::singleShot(0, this, &Display::stop);
                }
            }
        } else if (!auth->isDeferredAuthorization() && !stoppedHelper && !m_tearingDown
                   && status != Auth::HELPER_AUTH_ERROR) {
            stop();
        }
        advancePendingContext();
        startPendingLogin();
        if (auth == m_fingerprintAuth && m_pendingFingerprintRetryGeneration
            && m_pendingFingerprintRetryGeneration == auth->launchGeneration()) {
            m_pendingFingerprintRetryGeneration = 0;
            startFingerprint(m_contextSocket, m_contextSerial);
        }
        performPendingPower();
    }

    void Display::slotPrepareForSleep(bool sleeping) {
        if (m_tearingDown || m_contextCommitted || m_sessionAuth)
            return;
        if (sleeping) {
            invalidateProbe();
            m_fingerprintStartRequested = false;
            m_pendingFingerprintRetryGeneration = 0;
            if (m_pendingContext) {
                m_pendingFingerprintStart = false;
                m_pendingFingerprintCancelled = true;
            }
            if (m_fingerprintAuth->isActive() && isCurrentHelper(m_fingerprintAuth)
                && !m_methodWinnerChosen && !m_cancelledHelpers.contains(m_fingerprintAuth)) {
                m_attemptGate.cancel(AuthMethod::Fingerprint);
                disarmFingerprintDeadline();
                m_fingerprintTerminal = true;
                m_fingerprintTerminalState = FingerprintState::Cancelled;
                m_fingerprintTerminalDetail = QStringLiteral("system_suspend");
                stopAuthenticationHelper(m_fingerprintAuth);
            } else if (m_contextOptedIn && !m_pendingContext && !m_contextFinished
                       && !m_methodWinnerChosen && !m_fingerprintTerminal) {
                m_fingerprintTerminal = true;
                m_fingerprintTerminalState = FingerprintState::Cancelled;
                m_fingerprintTerminalDetail = QStringLiteral("system_suspend");
                publishMethodState(AuthenticationMethod::Fingerprint, FingerprintState::Cancelled,
                                   m_contextPhase, QStringLiteral("system_suspend"));
            }
        } else {
            // Never auto-start a hidden biometric scan on resume; require explicit
            // Retry or fresh user context selection while keeping password usable.
            m_fingerprintStartRequested = false;
            m_pendingFingerprintRetryGeneration = 0;
        }
    }

    void Display::slotRequestChanged() {
        Auth *auth = qobject_cast<Auth *>(sender());
        if (!auth || auth->method() != AuthMethod::Password || m_pendingContext || m_tearingDown
            || m_cancelledHelpers.contains(auth) || (auth->isDeferredAuthorization() && !isCurrentHelper(auth)))
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
        if (isCurrentHelper(auth) && auth == m_winnerAuth && m_contextCommitted && !m_tearingDown) {
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
                publishMethodState(AuthenticationMethod(auth->method()), FingerprintState::Failed,
                                   m_contextPhase, QStringLiteral("account_or_session_failed"));
                if (m_attemptSocket)
                    emit loginFailed(m_attemptSocket);
                finishMethodAttempt(false);
            }
        } else if (auth && auth->isDeferredAuthorization()) {
            return;
        }
        qDebug() << "Session started" << success;
        if (success)
            QTimer::singleShot(5000, m_greeter, &Greeter::stop);
    }
}
