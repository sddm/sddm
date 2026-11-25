/*
 * Main authentication application class
 * Copyright (C) 2013 Martin Bříza <mbriza@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "HelperApp.h"
#include "Backend.h"
#include "Configuration.h"
#include "UserSession.h"
#include "SafeDataStream.h"

#include "MessageHandler.h"
#include "VirtualTerminal.h"
#include "SignalHandler.h"

#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtNetwork/QLocalSocket>

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>

#if defined(Q_OS_LINUX)
#include <utmp.h>
#endif
#include <utmpx.h>
#include <QByteArray>
#include <signal.h>

namespace SDDM {
    HelperApp::HelperApp(int& argc, char** argv)
            : QCoreApplication(argc, argv)
            , m_backend(Backend::get(this))
            , m_session(new UserSession(this))
            , m_socket(new QLocalSocket(this)) {
        qInstallMessageHandler(HelperMessageHandler);
        SignalHandler *s = new SignalHandler(this);
        QObject::connect(s, &SignalHandler::sigtermReceived, m_session, [] {
            QCoreApplication::instance()->exit(-1);
        });

        QTimer::singleShot(0, this, SLOT(setUp()));
    }

    void HelperApp::setUp() {
        const QStringList args = QCoreApplication::arguments();
        QString server;
        int pos;

        if ((pos = args.indexOf(QStringLiteral("--socket"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            server = args[pos + 1];
        }

        if ((pos = args.indexOf(QStringLiteral("--id"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            m_id = QString(args[pos + 1]).toLongLong();
        }

        if ((pos = args.indexOf(QStringLiteral("--request-id"))) >= 0 && pos < args.length() - 1)
            m_requestId = args[pos + 1];
        if ((pos = args.indexOf(QStringLiteral("--method"))) >= 0 && pos < args.length() - 1) {
            bool ok = false;
            const int method = args[pos + 1].toInt(&ok);
            if (!ok || (method != qint32(AuthMethod::Password) && method != qint32(AuthMethod::Fingerprint))) {
                qCritical() << "Invalid authentication method";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            m_method = AuthMethod(method);
        }

        if ((pos = args.indexOf(QStringLiteral("--start"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            m_session->setPath(args[pos + 1]);
        }

        if ((pos = args.indexOf(QStringLiteral("--user"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            m_user = args[pos + 1];
        }

        if ((pos = args.indexOf(QStringLiteral("--display-server"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(Auth::HELPER_OTHER_ERROR);
                return;
            }
            m_session->setDisplayServerCommand(args[pos + 1]);
            m_backend->setDisplayServer(true);
        }

        if ((pos = args.indexOf(QStringLiteral("--autologin"))) >= 0) {
            m_backend->setAutologin(true);
        }
        m_deferredAuthorization = args.contains(QStringLiteral("--defer-auth"));

        if ((pos = args.indexOf(QStringLiteral("--greeter"))) >= 0) {
            m_backend->setGreeter(true);
        }

        if (server.isEmpty() || m_id <= 0 || QUuid(m_requestId).isNull()
            || (m_method == AuthMethod::Fingerprint && (m_user.isEmpty() || m_backend->isGreeter() || m_backend->isAutologin()))) {
            qCritical() << "Invalid authentication helper request";
            exit(Auth::HELPER_OTHER_ERROR);
            return;
        }
        m_requestedUser = m_user;
        m_backend->setFingerprint(m_method == AuthMethod::Fingerprint);

        connect(m_socket, &QLocalSocket::connected, this, &HelperApp::doAuth);
        connect(m_session, &UserSession::finished, this, &HelperApp::sessionFinished);
        m_socket->connectToServer(server, QIODevice::ReadWrite | QIODevice::Unbuffered);
    }

    void HelperApp::doAuth() {
        SafeDataStream str(m_socket);
        str << Msg::HELLO << m_id << m_requestId << m_method;
        str.send();
        if (str.status() != QDataStream::Ok)
            qCritical() << "Couldn't write initial message:" << str.status();

        if (!m_backend->start(m_user)) {
            QProcessEnvironment ignored;
            authenticated(QString(), ignored);

            // write failed login to btmp
            const QProcessEnvironment env = m_session->processEnvironment();
            const QString displayId = env.value(QStringLiteral("DISPLAY"));
            const QString vt = env.value(QStringLiteral("XDG_VTNR"));
            utmpLogin(vt, displayId, m_user, 0, false);

            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }

        Q_ASSERT(getuid() == 0);
        if (!m_backend->authenticate()) {
            QProcessEnvironment ignored;
            authenticated(QString(), ignored);

            // write failed login to btmp
            const QProcessEnvironment env = m_session->processEnvironment();
            const QString displayId = env.value(QStringLiteral("DISPLAY"));
            const QString vt = env.value(QStringLiteral("XDG_VTNR"));
            utmpLogin(vt, displayId, m_user, 0, false);

            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }

        m_user = m_backend->userName();
        if (m_user.isEmpty() || (!m_requestedUser.isEmpty() && m_user != m_requestedUser)) {
            error(QStringLiteral("Authentication user mismatch"), Auth::ERROR_AUTHENTICATION);
            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }

        if (!m_deferredAuthorization && !m_backend->accountManagement()) {
            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }

        QProcessEnvironment env;
        if (!authenticated(m_user, env)) {
            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }
        if (m_deferredAuthorization && !m_backend->accountManagement()) {
            exit(Auth::HELPER_AUTH_ERROR);
            return;
        }

        if (!m_session->path().isEmpty()) {
            env.insert(m_session->processEnvironment());
            m_session->setProcessEnvironment(env);

            if (!m_backend->openSession()) {
                sessionOpened(false);
                exit(Auth::HELPER_SESSION_ERROR);
                return;
            }

            sessionOpened(true);

            // write successful login to utmp/wtmp
            const QProcessEnvironment env = m_session->processEnvironment();
            const QString displayId = env.value(QStringLiteral("DISPLAY"));
            const QString vt = env.value(QStringLiteral("XDG_VTNR"));
            if (env.value(QStringLiteral("XDG_SESSION_CLASS")) != QLatin1String("greeter")) {
                // cache pid for session end
                utmpLogin(vt, displayId, m_user, m_session->processId(), true);
            }
        }
        else
            exit(Auth::HELPER_SUCCESS);
        return;
    }

    void HelperApp::sessionFinished(int status) {
        exit(status);
    }

    void HelperApp::info(const QString& message, Auth::Info type) {
        SafeDataStream str(m_socket);
        str << Msg::INFO << m_requestId << m_method << message << type;
        str.send();
        m_socket->waitForBytesWritten();
    }

    void HelperApp::error(const QString& message, Auth::Error type) {
        SafeDataStream str(m_socket);
        str << Msg::ERROR << m_requestId << m_method << message << type;
        str.send();
        m_socket->waitForBytesWritten();
    }

    Request HelperApp::request(const Request& request) {
        Msg m = Msg::MSG_UNKNOWN;
        Request response;
        SafeDataStream str(m_socket);
        str << Msg::REQUEST << m_requestId << m_method << request;
        str.send();
        str.receive();
        QString requestId;
        AuthMethod method = AuthMethod::Password;
        str >> m >> requestId >> method >> response;
        if (m != REQUEST || requestId != m_requestId || method != m_method) {
            response = Request();
            qCritical() << "Received a wrong opcode instead of REQUEST:" << m;
        }
        return response;
    }

    bool HelperApp::authenticated(const QString &user, QProcessEnvironment &environment) {
        Msg m = Msg::MSG_UNKNOWN;
        bool proceed = false;
        SafeDataStream str(m_socket);
        str << Msg::AUTHENTICATED << m_requestId << m_method << user;
        str.send();
        if (user.isEmpty())
            return false;
        str.receive();
        QString requestId;
        AuthMethod method = AuthMethod::Password;
        str >> m >> requestId >> method >> proceed;
        if (m == SESSION_AUTHORIZATION && requestId == m_requestId && method == m_method && proceed) {
            str >> environment >> m_cookie;
            return str.status() == QDataStream::Ok;
        }
        if (m != SESSION_AUTHORIZATION || requestId != m_requestId || method != m_method)
            qCritical() << "Rejected invalid session authorization";
        return false;
    }

    void HelperApp::sessionOpened(bool success) {
        Msg m = Msg::MSG_UNKNOWN;
        SafeDataStream str(m_socket);
        str << Msg::SESSION_STATUS << m_requestId << m_method << success;
        str.send();
        str.receive();
        QString requestId;
        AuthMethod method = AuthMethod::Password;
        str >> m >> requestId >> method;
        if (m != SESSION_STATUS || requestId != m_requestId || method != m_method) {
            qCritical() << "Received a wrong opcode instead of SESSION_STATUS:" << m;
        }
    }

    void HelperApp::displayServerStarted(const QString &displayName)
    {
        Msg m = Msg::MSG_UNKNOWN;
        SafeDataStream str(m_socket);
        str << Msg::DISPLAY_SERVER_STARTED << m_requestId << m_method << displayName;
        str.send();
        str.receive();
        QString requestId;
        AuthMethod method = AuthMethod::Password;
        str >> m >> requestId >> method;
        if (m != DISPLAY_SERVER_STARTED || requestId != m_requestId || method != m_method) {
            qCritical() << "Received a wrong opcode instead of DISPLAY_SERVER_STARTED:" << m;
        }
    }

    UserSession *HelperApp::session() {
        return m_session;
    }

    const QString& HelperApp::user() const {
        return m_user;
    }

    const QByteArray& HelperApp::cookie() const {
        return m_cookie;
    }

    HelperApp::~HelperApp() {
        Q_ASSERT(getuid() == 0);

        m_session->stop();
        m_backend->closeSession();

        // write logout to utmp/wtmp
        qint64 pid = m_session->cachedProcessId();
        if (pid < 0) {
            return;
        }
        QProcessEnvironment env = m_session->processEnvironment();
        if (env.value(QStringLiteral("XDG_SESSION_CLASS")) != QLatin1String("greeter")) {
            QString vt = env.value(QStringLiteral("XDG_VTNR"));
            QString displayId = env.value(QStringLiteral("DISPLAY"));
            utmpLogout(vt, displayId, pid);
        }
    }

    void HelperApp::utmpLogin(const QString &vt, const QString &displayName, const QString &user, qint64 pid, bool authSuccessful) {
        struct utmpx entry { };
        struct timeval tv;

        entry.ut_type = USER_PROCESS;
        entry.ut_pid = pid;

        // ut_line: vt
        if (!vt.isEmpty()) {
            QString tty = QStringLiteral("tty");
            tty.append(vt);
            QByteArray ttyBa = tty.toLocal8Bit();
            const char* ttyChar = ttyBa.constData();
            strncpy(entry.ut_line, ttyChar, sizeof(entry.ut_line) - 1);
        }

        // ut_host: displayName
        QByteArray displayBa = displayName.toLocal8Bit();
        const char* displayChar = displayBa.constData();
        strncpy(entry.ut_host, displayChar, sizeof(entry.ut_host) - 1);

        // ut_user: user
        QByteArray userBa = user.toLocal8Bit();
        const char* userChar = userBa.constData();
        strncpy(entry.ut_user, userChar, sizeof(entry.ut_user) -1);

        gettimeofday(&tv, NULL);
        entry.ut_tv.tv_sec = tv.tv_sec;
        entry.ut_tv.tv_usec = tv.tv_usec;

        // write to utmp
        setutxent();
        if (!pututxline (&entry))
            qWarning() << "Failed to write utmpx: " << strerror(errno);
        endutxent();

#if defined(Q_OS_LINUX)
        updwtmpx(authSuccessful ? "/var/log/wtmp" : "/var/log/btmp", &entry);
#endif
    }

    void HelperApp::utmpLogout(const QString &vt, const QString &displayName, qint64 pid) {
        struct utmpx entry { };
        struct timeval tv;

        entry.ut_type = DEAD_PROCESS;
        entry.ut_pid = pid;

        // ut_line: vt
        if (!vt.isEmpty()) {
            QString tty = QStringLiteral("tty");
            tty.append(vt);
            QByteArray ttyBa = tty.toLocal8Bit();
            const char* ttyChar = ttyBa.constData();
            strncpy(entry.ut_line, ttyChar, sizeof(entry.ut_line) - 1);
        }

        // ut_host: displayName
        QByteArray displayBa = displayName.toLocal8Bit();
        const char* displayChar = displayBa.constData();
        strncpy(entry.ut_host, displayChar, sizeof(entry.ut_host) - 1);

        gettimeofday(&tv, NULL);
        entry.ut_tv.tv_sec = tv.tv_sec;
        entry.ut_tv.tv_usec = tv.tv_usec;

        // write to utmp
        setutxent();
        if (!pututxline (&entry))
            qWarning() << "Failed to write utmpx: " << strerror(errno);
        endutxent();

#if defined(Q_OS_LINUX)
        // append to wtmp
        updwtmpx("/var/log/wtmp", &entry);
#elif defined(Q_OS_FREEBSD)
        pututxline(&entry);
#endif
    }
}

int main(int argc, char** argv) {
    SDDM::HelperApp app(argc, argv);
    return app.exec();
}
