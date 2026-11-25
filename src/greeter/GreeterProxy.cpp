/***************************************************************************
* Copyright (c) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "GreeterProxy.h"
#include "Messages.h"
#include "SessionModel.h"
#include "SocketWriter.h"

#include <QLocalSocket>
#include <QPointer>
#include <QUuid>

namespace SDDM {
    namespace {
        struct AuthenticationSnapshot {
            quint64 serial { 0 };
            QString user, requestId, sessionFile;
            quint32 sessionType { Session::UnknownSession };
            int sessionIndex { -1 };
            bool available { false }, busy { false }, completed { false }, succeeded { false };
            QString state { QStringLiteral("unavailable") }, detail { QStringLiteral("checking") };
        };

        // Decode only value fields. In particular, do not construct a Session (which
        // reads desktop files) until the entire stream transaction has committed.
        struct DaemonFrame {
            quint32 message { 0 }, value { 0 }, availability { 0 }, phase { 0 }, method { 0 }, state { 0 };
            quint32 sessionType { Session::UnknownSession };
            quint64 serial { 0 };
            QString requestId, user, sessionFile, detail;
        };

        bool matches(const DaemonFrame &frame, const AuthenticationSnapshot &snapshot) {
            return snapshot.serial && !snapshot.requestId.isEmpty()
                && frame.serial == snapshot.serial && frame.requestId == snapshot.requestId
                && frame.user == snapshot.user && frame.sessionType == snapshot.sessionType
                && frame.sessionFile == snapshot.sessionFile;
        }

        bool validIdentity(const DaemonFrame &frame) {
            return frame.sessionType <= Session::WaylandSession
                && (frame.sessionType == Session::UnknownSession ? frame.sessionFile.isEmpty()
                                                                 : !frame.sessionFile.isEmpty())
                && (frame.requestId.isEmpty() || !QUuid(frame.requestId).isNull());
        }

        bool validDetail(const QString &detail) {
            if (detail.size() > 128)
                return false;
            for (const QChar c : detail) {
                if (!(c >= QLatin1Char('a') && c <= QLatin1Char('z')) && c != QLatin1Char('_'))
                    return false;
            }
            return true;
        }

        bool locksSelection(quint32 phase) {
            return phase >= quint32(AuthenticationPhase::WinnerSelected)
                && phase < quint32(AuthenticationPhase::Finished);
        }
    }

    class GreeterProxyPrivate {
    public:
        enum Pending { None, Selection, Clear };
        QPointer<SessionModel> sessionModel;
        QLocalSocket *socket { nullptr };
        QString hostName;
        bool canPowerOff { false }, canReboot { false }, canSuspend { false }, canHibernate { false }, canHybridSleep { false };
        bool methodsAvailable { false }, controller { false }, methodsDisabled { false }, upgradeRequired { false };
        bool connected { false };
        bool correlated { false }, clearAfterCompletion { false }, passwordPending { false };
        bool legacyFeedbackEnabled { true }, suppressLegacyBatch { false };
        quint64 serialAllocator { 0 }, pendingSerial { 0 };
        Pending pending { None };
        AuthenticationSnapshot snapshot, previous;
        DaemonFrame retainedCompletion;
        bool hasRetainedCompletion { false };
        bool hasLegacyCompletion { false }, legacyCompletionSucceeded { false };

        bool submissionAllowed() const {
            return connected && controller && !upgradeRequired && !snapshot.busy && pending != Clear && !clearAfterCompletion;
        }

        void publish(GreeterProxy *proxy, const AuthenticationSnapshot &next, bool oldSubmissionAllowed) {
            const auto old = snapshot;
            snapshot = next;
            // Every getter already sees the entire new tuple before any observer runs.
            if (old.user != next.user) emit proxy->authenticationUserChanged(next.user);
            if (old.sessionIndex != next.sessionIndex) emit proxy->authenticationSessionIndexChanged(next.sessionIndex);
            if (old.requestId != next.requestId) emit proxy->fingerprintRequestIdChanged(next.requestId);
            if (old.available != next.available) emit proxy->fingerprintAvailableChanged(next.available);
            if (old.state != next.state) emit proxy->fingerprintStateChanged(next.state);
            if (old.detail != next.detail) emit proxy->fingerprintDetailCodeChanged(next.detail);
            if (old.busy != next.busy) emit proxy->authenticationBusyChanged(next.busy);
            if (oldSubmissionAllowed != submissionAllowed())
                emit proxy->authenticationSubmissionAllowedChanged(submissionAllowed());
        }

        quint64 nextSerial() {
            if (++serialAllocator == 0) ++serialAllocator;
            return serialAllocator;
        }

        void retainPrevious() {
            // Rapid A -> B -> A must not overwrite acknowledged A with unacknowledged B.
            if (pending == None) {
                previous = snapshot;
                hasRetainedCompletion = false;
                hasLegacyCompletion = false;
            }
        }
    };

    GreeterProxy::GreeterProxy(const QString &socket, QObject *parent) : QObject(parent), d(new GreeterProxyPrivate()) {
        d->socket = new QLocalSocket(this);
        d->socket->setReadBufferSize(1024 * 1024);
        connect(d->socket, &QLocalSocket::connected, this, &GreeterProxy::connected);
        connect(d->socket, &QLocalSocket::disconnected, this, &GreeterProxy::disconnected);
        connect(d->socket, &QLocalSocket::readyRead, this, &GreeterProxy::readyRead);
        connect(d->socket, &QLocalSocket::errorOccurred, this, &GreeterProxy::error);
        d->socket->connectToServer(socket);
    }

    GreeterProxy::~GreeterProxy() { delete d; }
    const QString &GreeterProxy::hostName() const { return d->hostName; }
    bool GreeterProxy::canPowerOff() const { return d->canPowerOff; }
    bool GreeterProxy::canReboot() const { return d->canReboot; }
    bool GreeterProxy::canSuspend() const { return d->canSuspend; }
    bool GreeterProxy::canHibernate() const { return d->canHibernate; }
    bool GreeterProxy::canHybridSleep() const { return d->canHybridSleep; }
    bool GreeterProxy::authenticationMethodsAvailable() const { return d->methodsAvailable; }
    const QString &GreeterProxy::authenticationUser() const { return d->snapshot.user; }
    int GreeterProxy::authenticationSessionIndex() const { return d->snapshot.sessionIndex; }
    bool GreeterProxy::fingerprintAvailable() const { return d->snapshot.available; }
    const QString &GreeterProxy::fingerprintDetailCode() const { return d->snapshot.detail; }
    bool GreeterProxy::authenticationBusy() const { return d->snapshot.busy; }
    bool GreeterProxy::authenticationSubmissionAllowed() const { return d->submissionAllowed(); }
    bool GreeterProxy::authenticationController() const { return d->controller; }
    const QString &GreeterProxy::fingerprintState() const { return d->snapshot.state; }
    const QString &GreeterProxy::fingerprintRequestId() const { return d->snapshot.requestId; }
    bool GreeterProxy::isConnected() const { return d->socket->state() == QLocalSocket::ConnectedState; }
    bool GreeterProxy::hasAuthenticationActivity() const {
        return d->correlated || d->pending != GreeterProxyPrivate::None || d->passwordPending;
    }

    void GreeterProxy::setSessionModel(SessionModel *model) {
        if (d->sessionModel) disconnect(d->sessionModel, nullptr, this, nullptr);
        d->sessionModel = model;
        if (model) {
            connect(model, &QAbstractItemModel::modelReset, this, &GreeterProxy::sessionModelChanged);
            connect(model, &QAbstractItemModel::rowsRemoved, this, &GreeterProxy::sessionModelChanged);
            connect(model, &QAbstractItemModel::rowsInserted, this, &GreeterProxy::sessionModelChanged);
            connect(model, &QAbstractItemModel::layoutChanged, this, &GreeterProxy::sessionModelChanged);
            connect(model, &QAbstractItemModel::dataChanged, this, &GreeterProxy::sessionModelChanged);
            connect(model, &QObject::destroyed, this, &GreeterProxy::sessionModelChanged);
        }
        sessionModelChanged();
    }

    void GreeterProxy::sessionModelChanged() {
        if (d->snapshot.user.isEmpty()) return;
        if (!d->sessionModel) {
            clearAuthenticationContext();
            return;
        }
        const int index = d->sessionModel->indexOfSession(d->snapshot.sessionType, d->snapshot.sessionFile);
        if (index < 0 && d->pending != GreeterProxyPrivate::Clear) {
            clearAuthenticationContext();
            return;
        }
        auto next = d->snapshot;
        next.sessionIndex = index;
        d->publish(this, next, d->submissionAllowed());
    }

    void GreeterProxy::powerOff() { SocketWriter(d->socket) << quint32(GreeterMessages::PowerOff); }
    void GreeterProxy::reboot() { SocketWriter(d->socket) << quint32(GreeterMessages::Reboot); }
    void GreeterProxy::suspend() { SocketWriter(d->socket) << quint32(GreeterMessages::Suspend); }
    void GreeterProxy::hibernate() { SocketWriter(d->socket) << quint32(GreeterMessages::Hibernate); }
    void GreeterProxy::hybridSleep() { SocketWriter(d->socket) << quint32(GreeterMessages::HybridSleep); }

    void GreeterProxy::login(const QString &user, const QString &password, int sessionIndex) {
        if (!d->submissionAllowed() || !d->sessionModel || user.isEmpty()) return;
        const Session &session = d->sessionModel->sessionAt(sessionIndex);
        if (!session.isValid()) return;
        const quint32 sessionType = session.type();
        const QString sessionFile = session.fileName();
        if (d->correlated) {
            if (d->snapshot.user != user || d->snapshot.sessionType != sessionType
                || d->snapshot.sessionFile != sessionFile || d->snapshot.completed)
                setAuthenticationContext(user, sessionIndex);
            if (d->snapshot.user != user || d->snapshot.sessionType != sessionType
                || d->snapshot.sessionFile != sessionFile || d->pending == GreeterProxyPrivate::Clear)
                return;
        }
        d->passwordPending = true;
        d->legacyFeedbackEnabled = !d->correlated;
        SocketWriter(d->socket) << quint32(GreeterMessages::Login) << user << password << sessionType << sessionFile;
        if (!d->correlated) {
            auto next = d->snapshot;
            next.user = user;
            next.sessionType = sessionType;
            next.sessionFile = sessionFile;
            next.sessionIndex = sessionIndex;
            next.completed = next.succeeded = false;
            d->publish(this, next, d->submissionAllowed());
        }
    }

    void GreeterProxy::setAuthenticationContext(const QString &user, int sessionIndex) {
        if (!d->controller || !d->sessionModel || !d->methodsAvailable || d->methodsDisabled
            || d->snapshot.busy || d->pending == GreeterProxyPrivate::Clear || user.isEmpty()) return;
        const Session &session = d->sessionModel->sessionAt(sessionIndex);
        if (!session.isValid()) return;
        const quint32 sessionType = session.type();
        const QString sessionFile = session.fileName();
        if (d->correlated && d->snapshot.user == user && d->snapshot.sessionType == sessionType
            && d->snapshot.sessionFile == sessionFile && !d->snapshot.completed) return;
        const bool allowed = d->submissionAllowed();
        d->retainPrevious();
        d->pendingSerial = d->nextSerial();
        d->pending = GreeterProxyPrivate::Selection;
        d->correlated = true;
        d->legacyFeedbackEnabled = false;
        d->passwordPending = false;
        AuthenticationSnapshot next;
        next.user = user;
        next.sessionType = sessionType;
        next.sessionFile = sessionFile;
        next.sessionIndex = sessionIndex;
        SocketWriter(d->socket) << quint32(GreeterMessages::SetAuthenticationContext)
                                << d->pendingSerial << quint32(1) << user << sessionType << sessionFile;
        d->publish(this, next, allowed);
    }

    void GreeterProxy::clearAuthenticationContext() {
        if (d->upgradeRequired || d->pending == GreeterProxyPrivate::Clear) return;
        if (!d->correlated && !d->methodsAvailable) {
            const bool allowed = d->submissionAllowed();
            d->pending = GreeterProxyPrivate::None;
            d->clearAfterCompletion = false;
            d->previous = AuthenticationSnapshot();
            d->hasRetainedCompletion = false;
            d->publish(this, AuthenticationSnapshot(), allowed);
            emit authenticationContextCleared(d->nextSerial());
            return;
        }
        const bool allowed = d->submissionAllowed();
        d->retainPrevious();
        d->pendingSerial = d->nextSerial();
        d->pending = GreeterProxyPrivate::Clear;
        d->clearAfterCompletion = false;
        if (d->correlated) d->passwordPending = false;
        SocketWriter(d->socket) << quint32(GreeterMessages::SetAuthenticationContext) << d->pendingSerial << quint32(0);
        d->publish(this, AuthenticationSnapshot(), allowed);
    }

    void GreeterProxy::disableAuthenticationMethods() {
        d->methodsDisabled = true;
        clearAuthenticationContext();
    }

    void GreeterProxy::setAuthenticationController(bool controller) {
        if (d->controller == controller) return;
        const bool allowed = d->submissionAllowed();
        d->controller = controller;
        emit authenticationControllerChanged(controller);
        if (allowed != d->submissionAllowed()) emit authenticationSubmissionAllowedChanged(d->submissionAllowed());
    }

    void GreeterProxy::startFingerprint(const QString &user, int sessionIndex) {
        if (!d->controller || !d->methodsAvailable || d->methodsDisabled || d->snapshot.busy
            || d->pending == GreeterProxyPrivate::Clear || !d->sessionModel) return;
        const Session &session = d->sessionModel->sessionAt(sessionIndex);
        if (!session.isValid() || user.isEmpty()) return;
        const quint32 sessionType = session.type();
        const QString sessionFile = session.fileName();
        if (!d->correlated || d->snapshot.user != user || d->snapshot.sessionType != sessionType
            || d->snapshot.sessionFile != sessionFile || d->snapshot.completed)
            setAuthenticationContext(user, sessionIndex);
        if (d->snapshot.user != user || d->snapshot.sessionType != sessionType
            || d->snapshot.sessionFile != sessionFile || d->pending == GreeterProxyPrivate::Clear) return;
        const quint64 serial = d->pending == GreeterProxyPrivate::Selection ? d->pendingSerial : d->snapshot.serial;
        if (!serial) return;
        SocketWriter(d->socket) << quint32(GreeterMessages::StartAuthenticationMethod)
                                << serial << quint32(AuthenticationMethod::Fingerprint);
    }

    void GreeterProxy::cancelFingerprint() {
        if (!d->controller || !d->methodsAvailable || d->methodsDisabled
            || d->pending == GreeterProxyPrivate::Clear || d->snapshot.completed) return;
        const quint64 serial = d->pending == GreeterProxyPrivate::Selection ? d->pendingSerial : d->snapshot.serial;
        if (!serial) return;
        SocketWriter(d->socket) << quint32(GreeterMessages::CancelAuthenticationMethod)
                                << serial << d->snapshot.requestId << quint32(AuthenticationMethod::Fingerprint);
    }

    void GreeterProxy::connected() {
        const bool allowed = d->submissionAllowed();
        d->connected = true;
        if (allowed != d->submissionAllowed()) emit authenticationSubmissionAllowedChanged(d->submissionAllowed());
        qDebug() << "Connected to the daemon.";
        SocketWriter(d->socket) << quint32(GreeterMessages::Connect);
        SocketWriter(d->socket) << quint32(GreeterMessages::QueryAuthenticationCapabilities);
    }

    void GreeterProxy::disconnected() {
        const bool allowed = d->submissionAllowed();
        const bool hadMethods = d->methodsAvailable;
        d->connected = false;
        d->methodsAvailable = false;
        d->correlated = false;
        d->upgradeRequired = false;
        d->pending = GreeterProxyPrivate::None;
        d->pendingSerial = d->serialAllocator = 0;
        d->previous = AuthenticationSnapshot();
        d->hasRetainedCompletion = d->clearAfterCompletion = d->passwordPending = false;
        d->hasLegacyCompletion = false;
        d->legacyFeedbackEnabled = false;
        d->publish(this, AuthenticationSnapshot(), allowed);
        if (hadMethods) emit authenticationMethodsAvailableChanged(false);
        emit socketDisconnected();
    }

    void GreeterProxy::error() { qCritical() << "Socket error: " << d->socket->errorString(); }

    void GreeterProxy::finishAuthentication(quint32 result, quint32 winningMethod) {
        if (d->snapshot.completed) return;
        const bool clearAgain = d->clearAfterCompletion && result != quint32(AuthenticationResult::Succeeded);
        const bool allowed = d->submissionAllowed();
        auto next = d->snapshot;
        next.completed = true;
        next.succeeded = result == quint32(AuthenticationResult::Succeeded);
        next.busy = result == quint32(AuthenticationResult::Succeeded) || d->clearAfterCompletion;
        if (result == quint32(AuthenticationResult::Failed) && winningMethod == quint32(AuthenticationMethod::Fingerprint)) {
            next.state = QStringLiteral("failed");
            next.detail = QStringLiteral("account_or_session_failed");
        }
        d->passwordPending = false;
        d->publish(this, next, allowed);
        emit authenticationFinished(next.requestId, next.user, next.sessionIndex, result, winningMethod);
        if (result == quint32(AuthenticationResult::Succeeded)) emit loginSucceeded();
        else emit loginFailed();
        if (clearAgain) clearAuthenticationContext();
    }

    void GreeterProxy::finishLegacyAuthentication(bool succeeded) {
        if (d->snapshot.completed) return;
        const bool clearAgain = d->clearAfterCompletion && !succeeded;
        auto next = d->snapshot;
        next.completed = true;
        next.succeeded = succeeded;
        next.busy = succeeded || d->clearAfterCompletion;
        d->passwordPending = false;
        d->publish(this, next, d->submissionAllowed());
        if (succeeded) emit loginSucceeded();
        else emit loginFailed();
        if (clearAgain) clearAuthenticationContext();
    }

    void GreeterProxy::readyRead() {
        QDataStream input(d->socket);
        if (d->socket->bytesAvailable() >= 1024 * 1024) { d->socket->abort(); return; }
        while (d->socket->bytesAvailable() >= qint64(sizeof(quint32))) {
            DaemonFrame frame;
            bool known = true;
            input.startTransaction();
            input >> frame.message;
            switch (DaemonMessages(frame.message)) {
                case DaemonMessages::Capabilities:
                case DaemonMessages::AuthenticationCapabilities: input >> frame.value; break;
                case DaemonMessages::HostName:
                case DaemonMessages::InformationMessage: input >> frame.detail; break;
                case DaemonMessages::LoginSucceeded:
                case DaemonMessages::LoginFailed: break;
                case DaemonMessages::AuthenticationContext:
                    input >> frame.serial >> frame.value >> frame.requestId >> frame.user
                          >> frame.sessionType >> frame.sessionFile >> frame.availability >> frame.phase >> frame.detail;
                    break;
                case DaemonMessages::AuthenticationMethodState:
                    input >> frame.serial >> frame.requestId >> frame.method >> frame.user
                          >> frame.sessionType >> frame.sessionFile >> frame.state >> frame.phase >> frame.detail;
                    break;
                case DaemonMessages::AuthenticationAttemptFinished:
                    input >> frame.serial >> frame.requestId >> frame.user >> frame.sessionType >> frame.sessionFile
                          >> frame.value >> frame.method;
                    break;
                default: known = false; break;
            }
            if (input.status() == QDataStream::ReadPastEnd) { input.rollbackTransaction(); return; }
            if (!known || input.status() != QDataStream::Ok || !input.commitTransaction()) {
                d->socket->abort();
                return;
            }
            const bool methodFrame = frame.message >= quint32(DaemonMessages::AuthenticationContext);
            if (methodFrame && (!frame.serial || !validIdentity(frame)
                || frame.phase > quint32(AuthenticationPhase::Finished) || !validDetail(frame.detail))) {
                d->socket->abort();
                return;
            }
            switch (DaemonMessages(frame.message)) {
                case DaemonMessages::Capabilities: {
                    d->canPowerOff = frame.value & Capability::PowerOff;
                    d->canReboot = frame.value & Capability::Reboot;
                    d->canSuspend = frame.value & Capability::Suspend;
                    d->canHibernate = frame.value & Capability::Hibernate;
                    d->canHybridSleep = frame.value & Capability::HybridSleep;
                    emit canPowerOffChanged(d->canPowerOff);
                    emit canRebootChanged(d->canReboot);
                    emit canSuspendChanged(d->canSuspend);
                    emit canHibernateChanged(d->canHibernate);
                    emit canHybridSleepChanged(d->canHybridSleep);
                    break;
                }
                case DaemonMessages::HostName:
                    d->hostName = frame.detail;
                    emit hostNameChanged(d->hostName);
                    break;
                case DaemonMessages::AuthenticationCapabilities: {
                    const bool allowed = d->submissionAllowed();
                    const bool oldMethods = d->methodsAvailable;
                    d->upgradeRequired = frame.value & 0x1;
                    d->methodsAvailable = !d->upgradeRequired && (frame.value & 0x2);
                    if (d->upgradeRequired) {
                        auto next = d->snapshot;
                        next.available = false;
                        next.detail = QStringLiteral("daemon_upgrade_required");
                        d->publish(this, next, allowed);
                    }
                    if (oldMethods != d->methodsAvailable) emit authenticationMethodsAvailableChanged(d->methodsAvailable);
                    break;
                }
                case DaemonMessages::AuthenticationContext: {
                    if (frame.value > 1 || frame.availability > quint32(AuthenticationAvailability::Unavailable)) {
                        d->socket->abort(); return;
                    }
                    if (!d->correlated && d->pending != GreeterProxyPrivate::Clear) break;
                    if (!frame.value) {
                        if (!d->correlated) {
                            if (d->pending != GreeterProxyPrivate::Clear || frame.serial != d->pendingSerial
                                || frame.detail != QLatin1String("session_committed")
                                || (frame.phase != quint32(AuthenticationPhase::SessionAuthorized)
                                    && frame.phase != quint32(AuthenticationPhase::Finished))
                                || d->previous.user.isEmpty() || frame.user != d->previous.user
                                || frame.sessionType != d->previous.sessionType
                                || frame.sessionFile != d->previous.sessionFile) break;
                            auto next = d->previous;
                            next.sessionIndex = d->sessionModel
                                ? d->sessionModel->indexOfSession(next.sessionType, next.sessionFile) : -1;
                            next.busy = true;
                            const bool allowed = d->submissionAllowed();
                            d->pending = GreeterProxyPrivate::None;
                            d->clearAfterCompletion = true;
                            const bool retained = d->hasLegacyCompletion;
                            const bool succeeded = d->legacyCompletionSucceeded;
                            d->hasLegacyCompletion = false;
                            d->previous = AuthenticationSnapshot();
                            // Never attach an unrequested UUID to a stock password transaction.
                            d->publish(this, next, allowed);
                            emit authenticationContextRejected(frame.serial, frame.detail);
                            if (retained) finishLegacyAuthentication(succeeded);
                            else if (next.completed && !next.succeeded) clearAuthenticationContext();
                            break;
                        }
                        if (d->pending == GreeterProxyPrivate::None || frame.serial != d->pendingSerial
                            || frame.detail != QLatin1String("session_committed")
                            || (frame.phase != quint32(AuthenticationPhase::SessionAuthorized)
                                && frame.phase != quint32(AuthenticationPhase::Finished))
                            || frame.requestId != d->previous.requestId || d->previous.requestId.isEmpty()
                            || frame.user != d->previous.user || frame.sessionType != d->previous.sessionType
                            || frame.sessionFile != d->previous.sessionFile) break;
                        const bool allowed = d->submissionAllowed();
                        const bool wasClear = d->pending == GreeterProxyPrivate::Clear;
                        auto next = d->previous;
                        next.sessionIndex = d->sessionModel
                            ? d->sessionModel->indexOfSession(next.sessionType, next.sessionFile) : -1;
                        const bool previousSucceeded = next.succeeded;
                        next.busy = true;
                        d->pending = GreeterProxyPrivate::None;
                        d->clearAfterCompletion = wasClear;
                        const bool retained = d->hasRetainedCompletion;
                        const auto completion = d->retainedCompletion;
                        d->previous = AuthenticationSnapshot();
                        d->hasRetainedCompletion = false;
                        d->publish(this, next, allowed);
                        emit authenticationContextRejected(frame.serial, frame.detail);
                        if (retained) finishAuthentication(completion.value, completion.method);
                        else if (next.completed && wasClear && !previousSucceeded)
                            clearAuthenticationContext();
                        break;
                    }
                    if (d->pending == GreeterProxyPrivate::Clear && frame.serial == d->pendingSerial) {
                        if (!frame.requestId.isEmpty() || !frame.user.isEmpty() || frame.sessionType != Session::UnknownSession
                            || !frame.sessionFile.isEmpty() || frame.availability != quint32(AuthenticationAvailability::Unavailable)
                            || frame.phase != quint32(AuthenticationPhase::Idle) || frame.detail != QLatin1String("cleared")) break;
                        const bool allowed = d->submissionAllowed();
                        d->pending = GreeterProxyPrivate::None;
                        d->correlated = false;
                        d->clearAfterCompletion = false;
                        d->previous = AuthenticationSnapshot();
                        d->hasRetainedCompletion = false;
                        d->hasLegacyCompletion = d->passwordPending = false;
                        d->legacyFeedbackEnabled = false;
                        d->suppressLegacyBatch = true;
                        AuthenticationSnapshot next;
                        next.detail = frame.detail;
                        d->publish(this, next, allowed);
                        emit authenticationContextCleared(frame.serial);
                        break;
                    }
                    const bool initialAck = d->pending == GreeterProxyPrivate::Selection && frame.serial == d->pendingSerial;
                    if ((!initialAck && !matches(frame, d->snapshot)) || frame.requestId.isEmpty()
                        || frame.user != d->snapshot.user || frame.sessionType != d->snapshot.sessionType
                        || frame.sessionFile != d->snapshot.sessionFile) break;
                    const bool allowed = d->submissionAllowed();
                    auto next = d->snapshot;
                    next.serial = frame.serial;
                    next.requestId = frame.requestId;
                    next.available = frame.availability == quint32(AuthenticationAvailability::Available);
                    next.detail = frame.detail;
                    if (!next.completed) next.busy = next.busy || locksSelection(frame.phase);
                    if (next.state == QLatin1String("unavailable") && next.available) next.state = QStringLiteral("ready");
                    if (initialAck) {
                        d->pending = GreeterProxyPrivate::None;
                        d->previous = AuthenticationSnapshot();
                        d->hasRetainedCompletion = false;
                    }
                    d->publish(this, next, allowed);
                    break;
                }
                case DaemonMessages::AuthenticationMethodState: {
                    if (frame.method > quint32(AuthenticationMethod::Fingerprint) || frame.state > quint32(FingerprintState::TimedOut)) {
                        d->socket->abort(); return;
                    }
                    if (!d->correlated || d->pending != GreeterProxyPrivate::None
                        || !matches(frame, d->snapshot) || d->snapshot.completed) break;
                    const bool allowed = d->submissionAllowed();
                    auto next = d->snapshot;
                    next.busy = next.busy || locksSelection(frame.phase);
                    if (frame.method == quint32(AuthenticationMethod::Fingerprint)) {
                        static const char *const states[] = { "unavailable", "ready", "scanning", "retry", "matched", "failed", "cancelled", "timed_out" };
                        next.state = QString::fromLatin1(states[frame.state]);
                        next.detail = frame.detail;
                        if (frame.state == quint32(FingerprintState::Unavailable)) next.available = false;
                        else if (frame.state == quint32(FingerprintState::Ready)) next.available = true;
                    }
                    const bool passwordFailed = frame.method == quint32(AuthenticationMethod::Password)
                        && frame.state == quint32(FingerprintState::Failed) && d->passwordPending;
                    if (passwordFailed) d->passwordPending = false;
                    d->publish(this, next, allowed);
                    if (passwordFailed) emit loginFailed();
                    break;
                }
                case DaemonMessages::AuthenticationAttemptFinished:
                    if (frame.value > quint32(AuthenticationResult::Cancelled) || frame.method > quint32(AuthenticationMethod::NoWinner)
                        || (frame.value == quint32(AuthenticationResult::Succeeded) && frame.method == quint32(AuthenticationMethod::NoWinner))) {
                        d->socket->abort(); return;
                    }
                    if (!d->correlated) break;
                    if (d->pending != GreeterProxyPrivate::None) {
                        if (!d->hasRetainedCompletion && matches(frame, d->previous)) {
                            d->retainedCompletion = frame;
                            d->hasRetainedCompletion = true;
                        }
                    } else if (matches(frame, d->snapshot)) finishAuthentication(frame.value, frame.method);
                    break;
                case DaemonMessages::InformationMessage:
                    if (!d->correlated && !d->upgradeRequired && d->legacyFeedbackEnabled && !d->suppressLegacyBatch
                        && d->pending == GreeterProxyPrivate::None && !d->clearAfterCompletion)
                        emit informationMessage(frame.detail);
                    break;
                case DaemonMessages::LoginSucceeded:
                case DaemonMessages::LoginFailed:
                    if (!d->correlated && !d->upgradeRequired && d->legacyFeedbackEnabled && !d->suppressLegacyBatch) {
                        const bool succeeded = DaemonMessages(frame.message) == DaemonMessages::LoginSucceeded;
                        if (d->pending == GreeterProxyPrivate::Clear) {
                            if (!d->hasLegacyCompletion && d->passwordPending) {
                                d->hasLegacyCompletion = true;
                                d->legacyCompletionSucceeded = succeeded;
                            }
                        } else {
                            finishLegacyAuthentication(succeeded);
                        }
                    }
                    break;
            }
        }
        d->suppressLegacyBatch = false;
    }
}
