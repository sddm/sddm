/*
 * Qt Authentication Library
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

#include "Auth.h"
#include "Constants.h"
#include "AuthMessages.h"
#include "SafeDataStream.h"
#include "PeerCredentials.h"

#include <QtCore/QProcess>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <QtQml/QtQml>

#include <memory>
#include <limits>

#include <unistd.h>
#if SDDM_TRUSTED_HELPER_PID
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <fcntl.h>
#include <cerrno>
#include <csignal>
#ifndef MFD_EXEC
#define MFD_EXEC 0x0010U
#endif
#ifndef SO_PEERPIDFD
#define SO_PEERPIDFD 77
#endif
#endif
#include <cstring>
#include <cstdint>

#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
extern "C" const unsigned char sddm_parallel_helper_begin[];
extern "C" const unsigned char sddm_parallel_helper_end[];
#endif

namespace SDDM {
    static void holdLegacySocket(QLocalSocket *socket) {
        socket->setReadBufferSize(1024);
    }
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
    // The only executable used in parallel mode is the helper linked into this
    // daemon. Seal the image before any child exists, closing pathname races.
    static int parallelRuntimeState = -1;
    static QSet<Auth *> parallelAuths;
    static QSet<Auth *> legacyAuths;
    // An exited or cancelled legacy launcher may have a delayed child. Never
    // reuse its identifier or close that child's socket before containment.
    static QSet<qint64> deniedLegacyIds;
#ifdef SDDM_PARALLEL_TEST
    static bool injectedQuarantineFailure = false;
    void testQuarantineFailure(bool enabled) { injectedQuarantineFailure = enabled; }
#else
    static constexpr bool injectedQuarantineFailure = false;
#endif
    static void quarantineLegacySocket(QLocalSocket *socket, bool signalPeer = true) {
        int peerPidFd = -1;
        socklen_t length = sizeof(peerPidFd);
        const bool identified = signalPeer && !injectedQuarantineFailure && getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERPIDFD, &peerPidFd, &length) == 0 &&
            length == sizeof(peerPidFd) && peerPidFd >= 0;
        if (identified) {
            syscall(SYS_pidfd_send_signal, peerPidFd, SIGKILL, nullptr, 0);
            close(peerPidFd);
        }
        // Other descendants may hold this same socket even after the peer is
        // signalled. Keep our end open until every peer holder disconnects.
        holdLegacySocket(socket);
    }
#ifdef SDDM_PARALLEL_TEST
    static int injectedFailure = 0;
    void testParallelFailure(int reason);
    bool testParallelProbeSettled() { return parallelRuntimeState >= 0; }
#else
    static constexpr int injectedFailure = 0;
#endif
    static int sealedParallelHelper() {
        const auto begin = reinterpret_cast<std::uintptr_t>(sddm_parallel_helper_begin);
        const auto end = reinterpret_cast<std::uintptr_t>(sddm_parallel_helper_end);
        if (end <= begin || end - begin > 64 * 1024 * 1024) return -1;
        const auto size = end - begin;
        if (injectedFailure == 1) return -1;
        int fd = -1;
        if (injectedFailure == 6) errno = EINVAL;
        else fd = syscall(SYS_memfd_create, "sddm-parallel-helper", MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_EXEC);
        if (fd < 0 && errno == EINVAL)
            fd = syscall(SYS_memfd_create, "sddm-parallel-helper", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (fd < 0) return -1;
        std::uintptr_t offset = 0;
        while (offset < size) {
            ssize_t n = write(fd, reinterpret_cast<const void *>(begin + offset), size - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { close(fd); return -1; }
            offset += n;
        }
        const int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
        if (injectedFailure == 2 || fcntl(fd, F_ADD_SEALS, seals) != 0) { close(fd); return -1; }
        const int actualSeals = injectedFailure == 7 ? -1 : fcntl(fd, F_GET_SEALS);
        if (actualSeals < 0 || (actualSeals & seals) != seals) { close(fd); return -1; }
        return fd;
    }
#ifdef SDDM_PARALLEL_TEST
    bool testEmbeddedImageSealed() {
        int fd = sealedParallelHelper();
        if (fd < 0) return false;
        errno = 0;
        const char bad = 0;
        const bool writeDenied = pwrite(fd, &bad, 1, 0) < 0 && errno == EPERM;
        errno = 0;
        const bool truncateDenied = ftruncate(fd, 0) < 0 && errno == EPERM;
        close(fd);
        return writeDenied && truncateDenied;
    }
#endif
    struct RuntimeListener {
        QPointer<QObject> context;
        std::function<void()> callback;
    };
    static QList<RuntimeListener> runtimeListeners;
    void registerParallelRuntimeListener(QObject *context, std::function<void()> callback) {
        runtimeListeners.append({context, std::move(callback)});
    }
    static void notifyRuntimeListeners() {
        for (auto it = runtimeListeners.begin(); it != runtimeListeners.end();) {
            if (!it->context) it = runtimeListeners.erase(it);
            else ++it;
        }
        const auto listeners = runtimeListeners;
        for (const auto &listener : listeners)
            if (listener.context) listener.callback();
    }
    class RuntimeProbe;
    static QPointer<RuntimeProbe> currentProbe;
    class RuntimeProbe : public QObject {
    public:
        explicit RuntimeProbe(int fd) : imageFd(fd) {
            timer.setSingleShot(true);
            connect(&timer, &QTimer::timeout, this, [this] { finish(false); });
            connect(&process, &QProcess::started, this, [this] { started(); });
            connect(&process, &QProcess::readyReadStandardOutput, this, [this] { ready(); });
            connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
                if (e == QProcess::FailedToStart) {
                    if (done) release();
                    else finish(false);
                }
            });
            connect(&process, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this,
                    [this](int status, QProcess::ExitStatus how) {
                        if (done) release();
                        else finish(accepted && how == QProcess::NormalExit && status == 0);
                    });
        }
        void begin() {
            timer.start(2000);
            if (injectedFailure == 3 || injectedFailure == 5) { finish(false); return; }
            const QString path = QStringLiteral("/proc/%1/fd/%2").arg(getpid()).arg(imageFd);
            process.start(path, {injectedFailure == 15 ? QStringLiteral("--parallel-probe-hang") :
                                 injectedFailure == 16 ? QStringLiteral("--parallel-probe-slow") : QStringLiteral("--parallel-probe")});
        }
        void abort() { finish(false, false); }
    private:
        void started() {
            if (done) { process.kill(); return; }
            pidfd = injectedFailure == 4 ? -1 : syscall(SYS_pidfd_open, process.processId(), 0);
            if (pidfd < 0) { finish(false); return; }
            int sockets[2] = {-1, -1};
            int own = -1, peer = -1;
            socklen_t peerLen = sizeof(peer);
            struct stat ownInfo = {}, peerInfo = {}, childInfo = {}, imageInfo = {}, executableInfo = {};
            bool peerIds = injectedFailure != 8 && socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) == 0 &&
                (own = syscall(SYS_pidfd_open, getpid(), 0)) >= 0 &&
                getsockopt(sockets[0], SOL_SOCKET, SO_PEERPIDFD, &peer, &peerLen) == 0 && peerLen == sizeof(peer) &&
                fstat(own, &ownInfo) == 0 && fstat(peer, &peerInfo) == 0 && fstat(pidfd, &childInfo) == 0 &&
                ownInfo.st_dev == peerInfo.st_dev && ownInfo.st_ino == peerInfo.st_ino &&
                (ownInfo.st_dev != childInfo.st_dev || ownInfo.st_ino != childInfo.st_ino);
            if (peer >= 0) close(peer);
            if (own >= 0) close(own);
            if (sockets[0] >= 0) close(sockets[0]);
            if (sockets[1] >= 0) close(sockets[1]);
            const auto executable = QByteArray("/proc/") + QByteArray::number(process.processId()) + "/exe";
            if (!peerIds || syscall(SYS_pidfd_send_signal, pidfd, 0, nullptr, 0) != 0 || injectedFailure == 14 ||
                fstat(imageFd, &imageInfo) != 0 || stat(executable.constData(), &executableInfo) != 0 ||
                imageInfo.st_dev != executableInfo.st_dev || imageInfo.st_ino != executableInfo.st_ino) {
                finish(false); return;
            }
            identityReady = true;
            ready();
        }
        void ready() {
            if (done) return;
            output += process.readAllStandardOutput();
            if (output.size() > 5 || (output.size() == 5 && output != "READY")) { finish(false); return; }
            if (!identityReady || output.size() != 5 || accepted) return;
            if (process.write("x", 1) != 1) { finish(false); return; }
            accepted = true;
        }
        void finish(bool ok, bool notify = true) {
            if (done) return;
            done = true;
            timer.stop();
            const bool running = process.state() != QProcess::NotRunning;
            if (running) process.kill();
            if (currentProbe == this) {
                currentProbe = nullptr;
                if (notify) {
                    // A valid probe is withheld until every legacy login
                    // attempt has ended, so its wrapper never races a reader.
                    parallelRuntimeState = ok ? (legacyAuths.isEmpty() ? 1 : -3) : 0;
                    if (parallelRuntimeState != -3) notifyRuntimeListeners();
                }
            }
            if (!running) release();
        }
        void release() {
            if (pidfd >= 0) { close(pidfd); pidfd = -1; }
            if (imageFd >= 0) { close(imageFd); imageFd = -1; }
            deleteLater();
        }
        QProcess process;
        QTimer timer;
        QByteArray output;
        int imageFd = -1;
        int pidfd = -1;
        bool identityReady = false, accepted = false, done = false;
    };
#ifdef SDDM_PARALLEL_TEST
    void testParallelFailure(int reason) {
        if (currentProbe) currentProbe->abort();
        injectedFailure = reason;
        parallelRuntimeState = -1;
    }
#endif
    bool parallelRuntimeAvailable() {
        if (parallelRuntimeState == -1) {
            parallelRuntimeState = -2;
            const int fd = sealedParallelHelper();
            if (fd < 0) { parallelRuntimeState = 0; notifyRuntimeListeners(); }
            else { currentProbe = new RuntimeProbe(fd); currentProbe->begin(); }
        }
        return parallelRuntimeState == 1;
    }
    static void legacyAttemptStarted(Auth *auth) {
        legacyAuths.insert(auth);
        if (parallelRuntimeState == 1) {
            disableParallelRuntime();
            parallelRuntimeState = -3;
        }
    }
    static void legacyAttemptFinished(Auth *auth) {
        legacyAuths.remove(auth);
        if (legacyAuths.isEmpty() && parallelRuntimeState == -3) {
            parallelRuntimeState = 1;
            notifyRuntimeListeners();
        }
    }
    void disableParallelRuntime() {
        if (parallelRuntimeState == 0) return;
        if (currentProbe) currentProbe->abort();
        QList<QPointer<Auth>> all;
        for (auto *auth : parallelAuths) all.append(QPointer<Auth>(auth));
        for (const auto &auth : all) if (auth && auth->parallelAttempt()) auth->cancelAuthentication();
        parallelRuntimeState = 0;
        notifyRuntimeListeners();
        for (const auto &auth : all) if (auth) emit auth->parallelUnavailable();
    }
#else
    static constexpr int injectedFailure = 0;
#endif
    class Auth::SocketServer : public QLocalServer {
        Q_OBJECT
    public slots:
        void handleNewConnection();
    public:
        static SocketServer *instance();

        QMap<qint64, Auth::Private*> helpers;
    private:
        SocketServer();
    };

    class Auth::Private : public QObject {
        Q_OBJECT
    public:
        Private(Auth *parent);
        ~Private();
        void setSocket(QLocalSocket *socket);
    public slots:
        void dataPending();
        void childExited(int exitCode, QProcess::ExitStatus exitStatus);
        void finishChildExit(int exitCode, QProcess::ExitStatus exitStatus);
        void childError(QProcess::ProcessError error);
        void requestFinished();
    public:
        AuthRequest *request { nullptr };
        QProcess *child { nullptr };
        QLocalSocket *socket { nullptr };
        QString displayServerCmd;
        QString sessionPath { };
        QString user { };
        QByteArray cookie { };
        bool autologin { false };
        bool greeter { false };
        bool fingerprint { false };
        bool passwordOnly { false };
        bool cancelled { false };
        bool sessionApproved { true };
        bool sessionReleased { false };
        bool parallelStarted { false };
        bool handshakeComplete { false };
        bool legacyAuthResolved { false };
        bool failureReported { false };
        bool failureExitTimedOut { false };
        QString failureProtocolError;
        bool legacyFinishing { false };
        int savedExitCode { 0 };
        QProcess::ExitStatus savedExitStatus { QProcess::NormalExit };
        QTimer cancelTimer;
        QTimer handshakeTimer;
        QTimer frameTimer;
        QTimer legacyDrainTimer;
        QTimer failureExitTimer;
        int parallelImageFd { -1 };
        int childPidFd { -1 };
        QProcessEnvironment environment { };
        qint64 id { 0 };
        static qint64 lastId;
        static qint64 nextId() {
            if (lastId == std::numeric_limits<qint64>::max()) return 0;
            return lastId++;
        }
    };

    qint64 Auth::Private::lastId = 1;



    Auth::SocketServer::SocketServer()
            : QLocalServer() {
        connect(this, &QLocalServer::newConnection, this, &Auth::SocketServer::handleNewConnection);
    }

    void Auth::SocketServer::handleNewConnection() {
        while (hasPendingConnections()) {
            QLocalSocket *socket = nextPendingConnection();
            Private *helper = nullptr;
#if SDDM_TRUSTED_HELPER_PID
            struct ucred peer = {};
            socklen_t len = sizeof(peer);
            if (getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &peer, &len) == 0 && len == sizeof(peer)) {
                for (Private *candidate : helpers) {
                    if (candidate->child->state() != QProcess::NotRunning &&
                        candidate->child->processId() == peer.pid && !candidate->socket) {
                        if (candidate->fingerprint || candidate->passwordOnly) {
                            if (candidate->cancelled) break;
                            struct stat expected = {}, actual = {}, childPid = {}, socketPid = {};
                            const auto executable = QByteArray("/proc/") + QByteArray::number(peer.pid) + "/exe";
                            int peerPidFd = -1;
                            socklen_t peerPidLen = sizeof(peerPidFd);
                            const bool valid = injectedFailure != 9 && candidate->parallelImageFd >= 0 && candidate->childPidFd >= 0 &&
                                getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERPIDFD, &peerPidFd, &peerPidLen) == 0 &&
                                peerPidLen == sizeof(peerPidFd) &&
                                syscall(SYS_pidfd_send_signal, candidate->childPidFd, 0, nullptr, 0) == 0 &&
                                syscall(SYS_pidfd_send_signal, peerPidFd, 0, nullptr, 0) == 0 &&
                                fstat(candidate->childPidFd, &childPid) == 0 && fstat(peerPidFd, &socketPid) == 0 &&
                                childPid.st_dev == socketPid.st_dev && childPid.st_ino == socketPid.st_ino &&
                                fstat(candidate->parallelImageFd, &expected) == 0 && stat(executable.constData(), &actual) == 0 &&
                                expected.st_dev == actual.st_dev && expected.st_ino == actual.st_ino;
                            if (peerPidFd >= 0) ::close(peerPidFd);
                            if (!valid) { disableParallelRuntime(); break; }
                        }
                        helper = candidate;
                        break;
                    }
                }
            }
#endif
            // SafeDataStream::receive blocks indefinitely on incomplete frames.
            // A hello has a fixed native length prefix and 12-byte payload.
            QPointer<Private> guard(helper);
            auto *timer = new QTimer(socket);
            const auto reject = [this, socket, guard, timer] {
                timer->stop();
                socket->disconnect(this);
                connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                if (guard && (guard->fingerprint || guard->passwordOnly)) {
                    if (guard->child->state() != QProcess::NotRunning) guard->child->kill();
                    socket->abort();
                    socket->deleteLater();
                } else {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                    quarantineLegacySocket(socket, bool(guard));
#else
                    // A legacy peer can open PAM on EOF. Keep unknown or
                    // malformed connections open until the peer disconnects.
                    holdLegacySocket(socket);
#endif
                }
            };
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, socket, reject);
            connect(socket, &QLocalSocket::disconnected, this, [socket] { socket->deleteLater(); });
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, timer, guard, reject] {
                if (socket->bytesAvailable() < qint64(sizeof(qint64))) return;
                const QByteArray prefix = socket->peek(sizeof(qint64));
                qint64 length = -1;
                std::memcpy(&length, prefix.constData(), sizeof(length));
                if (length != 12) { reject(); return; }
                if (socket->bytesAvailable() < qint64(sizeof(qint64)) + length) return;
                socket->read(sizeof(qint64));
                const QByteArray payload = socket->read(length);
                QDataStream stream(payload);
                Msg message = MSG_UNKNOWN;
                qint64 id = 0;
                stream >> message >> id;
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                auto *legacy = helpers.value(id, nullptr);
                if (stream.status() == QDataStream::Ok && deniedLegacyIds.contains(id) &&
                    (!legacy || !legacy->legacyFinishing)) {
                    timer->stop();
                    socket->disconnect(this);
                    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                    quarantineLegacySocket(socket);
                    return;
                }
#endif
                if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
                    reject(); return;
                }
#if SDDM_TRUSTED_HELPER_PID
                Private *helper = guard.data();
                // Preserve legacy helper wrappers. They can only claim a
                // non-parallel request; parallel peers require pidfd identity.
                if (!helper) {
                    helper = helpers.value(id, nullptr);
                    if (helper && (helper->fingerprint || helper->passwordOnly)) helper = nullptr;
                }
#else
                Private *helper = helpers.value(id, nullptr);
#endif
                if (!helper || helper->socket ||
                    (helper->child->state() == QProcess::NotRunning && !helper->legacyFinishing)) {
                    reject(); return;
                }
                const bool parallel = helper->fingerprint || helper->passwordOnly;
                if (helper->cancelled && !parallel) {
                    timer->stop();
                    socket->disconnect(this);
                    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                    quarantineLegacySocket(socket);
#else
                    holdLegacySocket(socket);
#endif
                    return;
                }
                if (stream.status() != QDataStream::Ok || !stream.atEnd() || id != helper->id ||
                    message != (parallel ? HELLO_V2 : HELLO)) {
                    reject(); return;
                }
                timer->stop();
                helper->handshakeTimer.stop();
                socket->disconnect(this);
                helper->setSocket(socket);
                if (socket->bytesAvailable() > 0)
                    helper->dataPending();
            });
            timer->start(5000);
        }
    }

    Auth::SocketServer* Auth::SocketServer::instance() {
        static std::unique_ptr<Auth::SocketServer> self;
        if (!self) {
            self.reset(new SocketServer());
            self->setSocketOptions(QLocalServer::UserAccessOption);
            self->listen(QStringLiteral("sddm-auth-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        }
        return self.get();
    }


    Auth::Private::Private(Auth *parent)
            : QObject(parent)
            , request(new AuthRequest(parent))
            , child(new QProcess(this))
            , id(nextId()) {
        if (id) SocketServer::instance()->helpers[id] = this;
        frameTimer.setSingleShot(true);
        frameTimer.setInterval(5000);
        connect(&frameTimer, &QTimer::timeout, this, [this] {
            if (child->state() != QProcess::NotRunning) child->kill();
        });
        legacyDrainTimer.setSingleShot(true);
        legacyDrainTimer.setInterval(100);
        connect(&legacyDrainTimer, &QTimer::timeout, this, [this] {
            legacyFinishing = false;
            finishChildExit(savedExitCode, savedExitStatus);
        });
        // A failed helper can close its socket before QProcess observes its
        // normal exit. Let that exit complete instead of killing it as a crash.
        // A reported failure is terminal and must not leave a stuck helper.
        failureExitTimer.setSingleShot(true);
        failureExitTimer.setInterval(1000);
        connect(&failureExitTimer, &QTimer::timeout, this, [this] {
            if (!cancelled && !sessionReleased && child->state() != QProcess::NotRunning) {
                failureExitTimedOut = true;
                child->kill();
            }
        });
        handshakeTimer.setSingleShot(true);
        handshakeTimer.setInterval(5000);
        connect(&handshakeTimer, &QTimer::timeout, this, [this] {
            if (!socket && child->state() != QProcess::NotRunning)
                child->kill();
        });
        cancelTimer.setSingleShot(true);
        cancelTimer.setInterval(1000);
        connect(&cancelTimer, &QTimer::timeout, this, [this] {
            if (cancelled && !sessionReleased && child->state() != QProcess::NotRunning)
                child->kill();
        });
        QProcessEnvironment env = child->processEnvironment();
        bool langEmpty = true;
        QFile localeFile(QStringLiteral("/etc/locale.conf"));
        if (localeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&localeFile);
            while (!in.atEnd()) {
                QStringList parts = in.readLine().split(QLatin1Char('='));
                if (parts.size() >= 2) {
                    env.insert(parts[0], parts[1]);
                    if (parts[0] == QLatin1String("LANG"))
                        langEmpty = false;
                }
            }
            localeFile.close();
        }
        if (langEmpty)
            env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
        child->setProcessEnvironment(env);
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        connect(child, &QProcess::started, this, [this] {
            if (parallelImageFd < 0) return;
            childPidFd = injectedFailure == 11 ? -1 : syscall(SYS_pidfd_open, child->processId(), 0);
            if (childPidFd < 0) disableParallelRuntime();
        });
#endif
        connect(child, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &Auth::Private::childExited);
        connect(child, &QProcess::errorOccurred, this, &Auth::Private::childError);
        connect(request, &AuthRequest::finished, this, &Auth::Private::requestFinished);
        connect(request, &AuthRequest::promptsChanged, parent, &Auth::requestChanged);
    }

    Auth::Private::~Private()
    {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        auto *auth = qobject_cast<Auth *>(parent());
        parallelAuths.remove(auth);
        if (legacyAuths.contains(auth) && !legacyAuthResolved) {
            deniedLegacyIds.insert(id);
            disableParallelRuntime();
            if (socket) {
                QLocalSocket *rejected = socket;
                socket = nullptr;
                rejected->disconnect(this);
                connect(rejected, &QLocalSocket::disconnected, rejected, &QObject::deleteLater);
                quarantineLegacySocket(rejected);
            }
        }
        legacyAttemptFinished(auth);
#endif
        SocketServer::instance()->helpers.remove(id);
        if (parallelImageFd >= 0) close(parallelImageFd);
        if (childPidFd >= 0) close(childPidFd);
    }


    void Auth::Private::setSocket(QLocalSocket *socket) {
        this->socket = socket;
        handshakeComplete = true;
        connect(socket, &QLocalSocket::readyRead, this, &Auth::Private::dataPending);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            QPointer<Private> self(this);
            QPointer<QLocalSocket> peer(socket);
            if (socket->bytesAvailable() > 0) dataPending();
            if (!self || !peer || this->socket != socket) return;
            const bool incompleteFrame = frameTimer.isActive() || socket->bytesAvailable() > 0;
            this->socket = nullptr;
            frameTimer.stop();
            socket->deleteLater();
            if (!sessionReleased && child->state() != QProcess::NotRunning) {
                if (failureReported && !cancelled && !incompleteFrame) {
                    if (!failureExitTimer.isActive()) failureExitTimer.start();
                } else {
                    child->kill();
                }
            }
        });
    }

    void Auth::Private::dataPending() {
        QPointer<Private> self(this);
        QPointer<QLocalSocket> peer(socket);
        QPointer<Auth> auth(qobject_cast<Auth*>(parent()));
        const auto valid = [this, self, peer, auth] { return self && peer && auth && this->socket == peer.data(); };
        if (!valid()) return;
        while (valid() && peer->bytesAvailable() > 0) {
            if (socket->bytesAvailable() < qint64(sizeof(qint64))) {
                if (!frameTimer.isActive()) frameTimer.start();
                return;
            }
            const QByteArray prefix = socket->peek(sizeof(qint64));
            qint64 length = -1;
            std::memcpy(&length, prefix.constData(), sizeof(length));
            if (length < 4 || length > 1024 * 1024) {
                child->kill();
                return;
            }
            if (socket->bytesAvailable() < qint64(sizeof(qint64)) + length) {
                if (!frameTimer.isActive()) frameTimer.start();
                return;
            }
            frameTimer.stop();
            socket->read(sizeof(qint64));
            const QByteArray payload = socket->read(length);
            QDataStream str(payload);
            Msg m = MSG_UNKNOWN;
            str >> m;
            if (str.status() != QDataStream::Ok) { child->kill(); return; }
            switch (m) {
                case ERROR: {
                    QString message;
                    Error type = ERROR_NONE;
                    str >> message >> type;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    Q_EMIT auth->error(message, type);
                    if (!valid()) return;
                    break;
                }
                case INFO: {
                    QString message;
                    Info type = INFO_NONE;
                    str >> message >> type;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    Q_EMIT auth->info(message, type);
                    if (!valid()) return;
                    break;
                }
                case REQUEST: {
                    Request r;
                    str >> r;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    request->setRequest(&r);
                    if (!valid()) return;
                    break;
                }
                case AUTHENTICATED: {
                    QString user;
                    str >> user;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    if (failureReported) {
                        failureProtocolError = Auth::tr("Authentication helper sent another result after reporting failure");
                        child->kill();
                        return;
                    }
                    if (!user.isEmpty()) {
                        // A launcher that already exited may have left a
                        // detached legacy child. Never release its session.
                        if (legacyFinishing) {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                            deniedLegacyIds.insert(id);
                            QLocalSocket *rejected = socket;
                            socket = nullptr;
                            rejected->disconnect(this);
                            connect(rejected, &QLocalSocket::disconnected, rejected, &QObject::deleteLater);
                            quarantineLegacySocket(rejected);
#endif
                            return;
                        }
                        auth->setUser(user);
                        if (!valid()) return;
                        if (!cancelled)
                            Q_EMIT auth->authentication(user, true);
                        if (!valid()) return;
                        sessionReleased = sessionApproved && !cancelled;
                        SafeDataStream reply(peer);
                        // Legacy peers have no approval field. In that protocol a
                        // denied session must end without receiving a release.
                        if (fingerprint || passwordOnly) {
                            reply << AUTHENTICATED << sessionReleased << environment << cookie;
                            reply.send();
                        } else if (sessionReleased) {
                            reply << AUTHENTICATED << environment << cookie;
                            reply.send();
                            if (!valid()) return;
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                            // The legacy session is released; other displays
                            // may use parallel mode. This display still gates
                            // new reader requests while the helper is alive.
                            legacyAuthResolved = true;
                            legacyAttemptFinished(auth.data());
                            if (!valid()) return;
#endif
                        } else {
                            child->kill();
                        }
                    }
                    else {
                        failureReported = true;
                        if (!cancelled && !sessionReleased && child->state() != QProcess::NotRunning)
                            failureExitTimer.start();
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
                        if (!parallelStarted && !greeter)
                            legacyAuthResolved = true;
#endif
                        if (!cancelled)
                            Q_EMIT auth->authentication(user, false);
                        if (!valid()) return;
                        if (fingerprint || passwordOnly) {
                            // This only acknowledges receipt of a failure. It
                            // carries no session approval or environment.
                            SafeDataStream reply(peer);
                            reply << AUTHENTICATED;
                            reply.send();
                            if (!valid()) return;
                        }
                    }
                    break;
                }
                case SESSION_STATUS: {
                    bool status;
                    str >> status;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    Q_EMIT auth->sessionStarted(status);
                    if (!valid()) return;
                    SafeDataStream reply(peer);
                    reply << SESSION_STATUS;
                    reply.send();
                    break;
                }
                case DISPLAY_SERVER_STARTED: {
                    QString displayName;
                    str >> displayName;
                    if (str.status() != QDataStream::Ok) { child->kill(); return; }
                    Q_EMIT auth->displayServerReady(displayName);
                    if (!valid()) return;
                    SafeDataStream reply(peer);
                    reply << DISPLAY_SERVER_STARTED;
                    reply.send();
                    break;
                }
                default: {
                    Q_EMIT auth->error(QStringLiteral("Auth: Unexpected value received: %1").arg(m), ERROR_INTERNAL);
                    if (!valid()) return;
                }
            }
        }
    }

    void Auth::Private::childExited(int exitCode, QProcess::ExitStatus exitStatus) {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (!parallelStarted && !greeter && !legacyAuthResolved) {
            deniedLegacyIds.insert(id);
            legacyFinishing = true;
            savedExitCode = exitCode;
            savedExitStatus = exitStatus;
            if (socket && socket->bytesAvailable() > 0) dataPending();
            legacyDrainTimer.start();
            return;
        }
#endif
        finishChildExit(exitCode, exitStatus);
    }

    void Auth::Private::finishChildExit(int exitCode, QProcess::ExitStatus exitStatus) {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (parallelStarted && !handshakeComplete && !cancelled) disableParallelRuntime();
        if (!parallelStarted && !greeter && !legacyAuthResolved) disableParallelRuntime();
        legacyAttemptFinished(qobject_cast<Auth *>(parent()));
#endif
        cancelTimer.stop();
        handshakeTimer.stop();
        frameTimer.stop();
        failureExitTimer.stop();
        if (parallelImageFd >= 0) { close(parallelImageFd); parallelImageFd = -1; }
        if (childPidFd >= 0) { close(childPidFd); childPidFd = -1; }
        if (socket) {
            QLocalSocket *finishedSocket = socket;
            socket = nullptr;
            finishedSocket->disconnect(this);
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
            if (!parallelStarted && !greeter && !legacyAuthResolved) {
                connect(finishedSocket, &QLocalSocket::disconnected, finishedSocket, &QObject::deleteLater);
                quarantineLegacySocket(finishedSocket);
            } else
#endif
                finishedSocket->deleteLater();
        }
        QPointer<Auth> auth(qobject_cast<Auth*>(parent()));
        if (cancelled) {
            Q_EMIT auth->finished(HELPER_AUTH_ERROR);
            return;
        }
        QString internalError;
        if (!failureProtocolError.isEmpty()) {
            internalError = failureProtocolError;
        } else if (failureExitTimedOut) {
            internalError = Auth::tr("Authentication helper did not finish after reporting failure");
        } else if (exitStatus != QProcess::NormalExit) {
            qWarning("Auth: sddm-helper (%s) crashed",
                     qPrintable(child->arguments().join(QLatin1Char(' '))));
            internalError = child->errorString();
        } else if (failureReported && exitCode != HELPER_AUTH_ERROR) {
            internalError = Auth::tr("Authentication helper returned an unexpected exit code after reporting failure");
        }
        if (!internalError.isEmpty()) {
            Q_EMIT auth->error(internalError, ERROR_INTERNAL);
            if (auth) Q_EMIT auth->finished(HELPER_OTHER_ERROR);
            return;
        }

        if (exitCode == HELPER_SUCCESS)
            qDebug() << "Auth: sddm-helper exited successfully";
        else
            qWarning("Auth: sddm-helper exited with %d", exitCode);

        Q_EMIT auth->finished(static_cast<Auth::HelperExitStatus>(exitCode));
    }

    void Auth::Private::childError(QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            handshakeTimer.stop();
            frameTimer.stop();
            cancelTimer.stop();
            failureExitTimer.stop();
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
            if (parallelStarted || (!greeter && !legacyAuthResolved)) disableParallelRuntime();
            legacyAttemptFinished(qobject_cast<Auth *>(parent()));
#endif
            if (parallelImageFd >= 0) { close(parallelImageFd); parallelImageFd = -1; }
            if (childPidFd >= 0) { close(childPidFd); childPidFd = -1; }
            if (!cancelled) Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
            Q_EMIT qobject_cast<Auth*>(parent())->finished(cancelled ? HELPER_AUTH_ERROR : HELPER_OTHER_ERROR);
            return;
        }
        // Crashed is reported once from finished(), where the exit status and
        // the bounded failed-helper cleanup outcome are both available.
        if (!cancelled && error != QProcess::Crashed)
            Q_EMIT qobject_cast<Auth*>(parent())->error(child->errorString(), ERROR_INTERNAL);
    }

    void Auth::Private::requestFinished() {
        QPointer<Private> self(this);
        QPointer<QLocalSocket> peer(socket);
        if (cancelled || !peer || !peer->isOpen()) {
            request->setRequest();
            return;
        }
        Request r = request->request();
        if (!self || !peer || socket != peer.data() || cancelled) return;
        SafeDataStream str(peer);
        str << REQUEST << r;
        str.send();
        if (!self) return;
        request->setRequest();
    }


    Auth::Auth(const QString &user, const QString &session, bool autologin, QObject *parent, bool verbose)
            : QObject(parent)
            , d(new Private(this)) {
        setUser(user);
        setAutologin(autologin);
        setSession(session);
        setVerbose(verbose);
    }

    Auth::Auth(QObject* parent)
            : QObject(parent)
            , d(new Private(this)) {
    }

    Auth::~Auth() {
        stop();
        delete d;
    }

    void Auth::registerTypes() {
        qmlRegisterAnonymousType<AuthPrompt>("Auth", 1);
        qmlRegisterAnonymousType<AuthRequest>("Auth", 1);
        qmlRegisterType<Auth>("Auth", 1, 0, "Auth");
    }

    bool Auth::autologin() const {
        return d->autologin;
    }

    bool Auth::isGreeter() const
    {
        return d->greeter;
    }

    const QByteArray& Auth::cookie() const {
        return d->cookie;
    }

    const QString &Auth::session() const {
        return d->sessionPath;
    }

    const QString &Auth::user() const {
        return d->user;
    }

    bool Auth::verbose() const {
        return d->child->processChannelMode() == QProcess::ForwardedChannels;
    }

    AuthRequest *Auth::request() {
        return d->request;
    }

    bool Auth::isActive() const {
        return d->legacyFinishing || d->child->state() != QProcess::NotRunning;
    }

    void Auth::insertEnvironment(const QProcessEnvironment &env) {
        d->environment.insert(env);
    }

    void Auth::insertEnvironment(const QString &key, const QString &value) {
        d->environment.insert(key, value);
    }

    void Auth::setCookie(const QByteArray& cookie) {
        if (cookie != d->cookie) {
            d->cookie = cookie;
            Q_EMIT cookieChanged();
        }
    }

    void Auth::setUser(const QString &user) {
        if (user != d->user) {
            d->user = user;
            Q_EMIT userChanged();
        }
    }

    void Auth::setAutologin(bool on) {
        if (on != d->autologin) {
            d->autologin = on;
            Q_EMIT autologinChanged();
        }
    }

    void Auth::setGreeter(bool on)
    {
        if (on != d->greeter) {
            d->greeter = on;
            Q_EMIT greeterChanged();
        }
    }

    void Auth::setDisplayServerCommand(const QString &command)
    {
        if (d->displayServerCmd != command) {
            d->displayServerCmd = command;
            Q_EMIT displayServerCommandChanged();
        }
    }

    void Auth::setSession(const QString& path) {
        if (path != d->sessionPath) {
            d->sessionPath = path;
            Q_EMIT sessionChanged();
        }
    }

    void Auth::setVerbose(bool on) {
        if (on != verbose()) {
            if (on)
                d->child->setProcessChannelMode(QProcess::ForwardedChannels);
            else
                d->child->setProcessChannelMode(QProcess::SeparateChannels);
            Q_EMIT verboseChanged();
        }
    }

    bool Auth::fingerprint() const { return d->fingerprint; }
    bool Auth::parallelAttempt() const { return d->parallelStarted; }

    bool Auth::cancelled() const { return d->cancelled; }

    void Auth::setFingerprint(bool enabled) { d->fingerprint = enabled; }

    void Auth::setPasswordOnly(bool enabled) { d->passwordOnly = enabled; }

    void Auth::setSessionApproved(bool approved) { d->sessionApproved = approved; }

    void Auth::cancelAuthentication() {
        // Never terminate an established session through the auth-only API.
        if (d->sessionReleased) return;
        d->cancelled = true;
        d->failureExitTimer.stop();
        d->sessionApproved = false;
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (!d->parallelStarted && !d->greeter) {
            deniedLegacyIds.insert(d->id);
            if (d->socket) {
                QLocalSocket *rejected = d->socket;
                d->socket = nullptr;
                rejected->disconnect(d);
                connect(rejected, &QLocalSocket::disconnected, rejected, &QObject::deleteLater);
                quarantineLegacySocket(rejected);
            }
        }
#else
        if (!d->parallelStarted && !d->greeter && d->socket) {
            QLocalSocket *rejected = d->socket;
            d->socket = nullptr;
            rejected->disconnect(d);
            connect(rejected, &QLocalSocket::disconnected, rejected, &QObject::deleteLater);
            holdLegacySocket(rejected);
        }
#endif
        if (!isActive()) return;
        d->child->terminate();
        d->cancelTimer.start();
    }

    void Auth::start() {
        if (isActive()) return;
#if !SDDM_TRUSTED_HELPER_PID || !defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (d->fingerprint || d->passwordOnly) {
            Q_EMIT error(QStringLiteral("Parallel authentication requires trusted helper peer credentials"), ERROR_INTERNAL);
            Q_EMIT finished(HELPER_OTHER_ERROR);
            return;
        }
#endif
        // A completed or cancelled helper may leave a late descendant.
        // Give every attempt a fresh identifier and stop rather than wrap.
        const qint64 next = Private::nextId();
        if (!next) {
            Q_EMIT error(QStringLiteral("Authentication identifiers exhausted"), ERROR_INTERNAL);
            Q_EMIT finished(HELPER_OTHER_ERROR);
            return;
        }
        SocketServer::instance()->helpers.remove(d->id);
        d->id = next;
        SocketServer::instance()->helpers[d->id] = d;
        d->parallelStarted = d->fingerprint || d->passwordOnly;
        d->handshakeComplete = false;
        d->legacyAuthResolved = false;
        d->failureReported = false;
        d->failureExitTimedOut = false;
        d->failureProtocolError.clear();
        d->failureExitTimer.stop();
        d->legacyFinishing = false;
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (d->parallelStarted) {
            parallelAuths.insert(this);
            legacyAuths.remove(this);
        } else {
            parallelAuths.remove(this);
            if (!d->greeter) legacyAttemptStarted(this);
        }
#endif
        d->cancelTimer.stop();
        d->cancelled = false;
        d->sessionApproved = true;
        d->sessionReleased = false;
        QStringList args;
        args << QStringLiteral("--socket") << SocketServer::instance()->fullServerName();
        args << QStringLiteral("--id") << QString::number(d->id);
        if (!d->sessionPath.isEmpty())
            args << QStringLiteral("--start") << d->sessionPath;
        if (!d->user.isEmpty())
            args << QStringLiteral("--user") << d->user;
        if (d->autologin)
            args << QStringLiteral("--autologin");
        if (d->fingerprint || d->passwordOnly) {
            args << QStringLiteral("--parallel-auth-v2");
#ifdef SDDM_PARALLEL_TEST
            if (injectedFailure == 12) args << QStringLiteral("--test-exit-before-hello");
#endif
        }
        if (d->fingerprint)
            args << QStringLiteral("--fingerprint");
        else if (d->passwordOnly)
            args << QStringLiteral("--password-only");
        if (!d->displayServerCmd.isEmpty())
            args << QStringLiteral("--display-server") << d->displayServerCmd;
        if (d->greeter)
            args << QStringLiteral("--greeter");
        QString program = QStringLiteral("%1/sddm-helper").arg(QStringLiteral(LIBEXEC_INSTALL_DIR));
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
        if (d->parallelStarted) {
            if (!parallelRuntimeAvailable()) {
                Q_EMIT error(QStringLiteral("Parallel helper is unavailable"), ERROR_INTERNAL);
                Q_EMIT finished(HELPER_OTHER_ERROR);
                return;
            }
            d->parallelImageFd = sealedParallelHelper();
            if (d->parallelImageFd < 0) {
                disableParallelRuntime();
                Q_EMIT error(QStringLiteral("Parallel helper could not be sealed"), ERROR_INTERNAL);
                Q_EMIT finished(HELPER_OTHER_ERROR);
                return;
            }
            program = injectedFailure == 10 ? QStringLiteral("/nonexistent-sddm-fixture") :
                QStringLiteral("/proc/%1/fd/%2").arg(getpid()).arg(d->parallelImageFd);
        }
#endif
        d->handshakeTimer.start();
        d->child->start(program, args);
    }

    void Auth::stop() {
        d->failureExitTimer.stop();
        if (d->child->state() == QProcess::NotRunning) {
            return;
        }

        d->child->terminate();

        // wait for finished
        if (!d->child->waitForFinished(5000))
            d->child->kill();
    }
}

#include "Auth.moc"
