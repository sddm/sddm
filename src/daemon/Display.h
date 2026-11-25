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

#ifndef SDDM_DISPLAY_H
#define SDDM_DISPLAY_H

#include <QObject>
#include <QPointer>
#include <QDir>
#include <QSet>
#include <QHash>
#include <QDBusObjectPath>

#include "Auth.h"
#include "AuthAttemptGate.h"
#include "Session.h"

class QDBusServiceWatcher;
class QDBusPendingCallWatcher;
class QLocalSocket;
class QTimer;

namespace SDDM {
    class Authenticator;
    class DisplayServer;
    class Seat;
    class SocketServer;
    class Greeter;

    class Display : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Display)
    public:
        enum DisplayServerType {
            X11DisplayServerType,
            X11UserDisplayServerType,
            WaylandDisplayServerType
        };
        Q_ENUM(DisplayServerType)

        static DisplayServerType defaultDisplayServerType();
        explicit Display(Seat *parent, DisplayServerType serverType);
        ~Display();

        DisplayServerType displayServerType() const;
        DisplayServer *displayServer() const;

        int terminalId() const;

        const QString &name() const;

        QString sessionType() const;
        QString reuseSessionId() const { return m_reuseSessionId; }

        Seat *seat() const;

    public slots:
        bool start();
        void stop();

        void login(QLocalSocket *socket, const QString &user, const QString &password, const Session &session);
        void setAuthenticationContext(QLocalSocket *socket, quint64 serial, bool hasContext,
                                      const QString &user, const Session &session);
        void startFingerprint(QLocalSocket *socket, quint64 serial);
        void cancelFingerprint(QLocalSocket *socket, quint64 serial, const QString &requestId);
        void displayServerStarted();

    signals:
        void stopped();
        void displayServerFailed();

        void loginFailed(QLocalSocket *socket);
        void loginSucceeded(QLocalSocket *socket);

    private:
        friend class DaemonContextTest;
        Display(Seat *parent, DisplayServerType serverType, int terminalId);
        QString findGreeterTheme() const;
        bool findSessionEntry(const QStringList &dirPaths, const QString &name) const;

        virtual bool startAuth(const QString &user, const QString &password,
                       const Session &session, AuthMethod method = AuthMethod::Password);
        void stopInternal(bool destroying);
        void cancelMethodAttempt(bool destroying = false);
        void authorizeWinner();
        void finishMethodAttempt(bool success);
        void publishMethodState(AuthenticationMethod method, FingerprintState state,
                                AuthenticationPhase phase, const QString &detailCode);
        virtual void probeFingerprintAvailability();
        void resolveFingerprintServiceOwner(quint64 generation, bool allowActivation);
        void setFingerprintAvailability(quint64 serial, const QString &requestId, bool available,
                                       const QString &detailCode);
        void startPendingLogin();
        void advancePendingContext();
        void beginMethodAttempt(QLocalSocket *socket, const QString &user, const Session &session);
        void clearPendingLogin();
        void invalidateProbe();
        void probeEnrolledDevices(const QList<QDBusObjectPath> &devices, int index, bool hadError,
                                  const QString &owner, quint64 generation);
        void confirmFingerprintAvailability(const QString &owner, quint64 generation);
        void stopAuthenticationHelper(Auth *auth, bool destroying = false);
        void disarmFingerprintDeadline();
        void armFingerprintDeadline(Auth *auth);
        bool isCurrentHelper(Auth *auth) const;
        void retireSocket(QLocalSocket *socket);
        void requestPower(GreeterMessages action);
        void performPendingPower();

        void startSocketServerAndGreeter();
        bool handleAutologinFailure();

        QPointer<QLocalSocket> m_contextSocket;
        quint64 m_contextSerial { 0 };
        quint64 m_pendingContextSerial { 0 };
        QString m_contextUser;
        QPointer<QLocalSocket> m_pendingContextSocket;
        QString m_pendingContextUser;
        Session m_pendingContextSession;
        bool m_pendingContext { false };
        bool m_pendingHasContext { false };
        bool m_pendingFingerprintStart { false };
        bool m_pendingFingerprintCancelled { false };
        bool m_fingerprintStartRequested { false };
        QHash<QLocalSocket *, quint64> m_selectionSerials;
        Session m_contextSession;
        bool m_contextOptedIn { false };
        bool m_contextCommitted { false };
        bool m_contextFinished { false };
        AuthenticationPhase m_contextPhase { AuthenticationPhase::Idle };
        bool m_tearingDown { false };
        int m_pendingPower { -1 };
        quint64 m_probeGeneration { 0 };
        QString m_probeServiceOwner;
        QDBusServiceWatcher *m_fprintServiceWatcher { nullptr };
        AuthenticationAvailability m_fingerprintAvailability { AuthenticationAvailability::Unknown };
        QSet<QDBusPendingCallWatcher *> m_probeWatchers;
        QPointer<QTimer> m_probeDeadline;
        quint64 m_passwordGeneration { 0 };
        quint64 m_fingerprintGeneration { 0 };
        // Generation-bound explicit retries share the current socket/context;
        // invalidation or winner selection discards them before any relaunch.
        quint64 m_pendingPasswordGeneration { 0 };
        quint64 m_pendingFingerprintRetryGeneration { 0 };
        bool m_fingerprintAwaitingAuthentication { false };
        bool m_fingerprintTerminal { false };
        FingerprintState m_fingerprintTerminalState { FingerprintState::Failed };
        QString m_fingerprintTerminalDetail;
        struct MethodSnapshot {
            bool valid { false };
            FingerprintState state { FingerprintState::Unavailable };
            AuthenticationPhase phase { AuthenticationPhase::Idle };
            QString detail;
        };
        MethodSnapshot m_passwordSnapshot;
        MethodSnapshot m_fingerprintSnapshot;
        QPointer<QTimer> m_fingerprintDeadline;
        DisplayServerType m_displayServerType = X11DisplayServerType;

        bool m_started { false };

        int m_terminalId = -1;
        int m_sessionTerminalId = 0;

        QString m_passPhrase;
        QString m_sessionName;
        QString m_reuseSessionId;
        QString m_attemptReuseSessionId;
        bool m_reuseSessionSelected { false };

        Session m_autologinSession;

        Auth *m_auth { nullptr };
        Auth *m_fingerprintAuth { nullptr };
        QPointer<Auth> m_winnerAuth;
        QPointer<Auth> m_loserAuth;
        QPointer<Auth> m_sessionAuth;
        QSet<Auth *> m_cancelledHelpers;
        AuthAttemptGate m_attemptGate;
        QPointer<QLocalSocket> m_attemptSocket;
        QString m_requestId;
        QString m_attemptUser;
        Session m_attemptSession;
        bool m_methodAttemptActive { false };
        bool m_methodWinnerChosen { false };
        bool m_waitingForLoser { false };
        bool m_pendingLogin { false };
        QPointer<QLocalSocket> m_pendingSocket;
        QString m_pendingUser;
        QString m_pendingPassword;
        Session m_pendingSession;
        DisplayServer *m_displayServer { nullptr };
        Seat *m_seat { nullptr };
        SocketServer *m_socketServer { nullptr };
        QPointer<QLocalSocket> m_socket;
        Greeter *m_greeter { nullptr };

    private slots:
        void slotPrepareForSleep(bool sleeping);
        void slotRequestChanged();
        void slotAuthenticationFinished(const QString &user, bool success);
        void slotAuthenticationMethodState(const QString &requestId, AuthMethod method,
                                           FingerprintState state, const QString &detailCode);
        void slotSessionStarted(bool success);
        void slotHelperFinished(Auth::HelperExitStatus status);
        void slotAuthInfo(const QString &message, Auth::Info info);
        void slotAuthError(const QString &message, Auth::Error error);
    };
}

#endif // SDDM_DISPLAY_H
