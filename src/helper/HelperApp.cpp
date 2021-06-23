/*
 * Main authentication application class
 * Copyright (c) 2013 Martin Bříza <mbriza@redhat.com>
 * Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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
#include "Configuration.h"

#include "MessageHandler.h"
#include "VirtualTerminal.h"

#include "Utils.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtNetwork/QLocalSocket>

#include <iostream>
#include <unistd.h>
#include <locale.h>
#include <sys/socket.h>
#include <sys/time.h>

namespace SDDM {
    HelperApp::HelperApp(int& argc, char** argv)
            : QCoreApplication(argc, argv)
            , m_backend(Backend::get(this))
            , m_session(new UserSession(this))
            , m_socket(new QLocalSocket(this)) {
        qInstallMessageHandler(HelperMessageHandler);
        QTimer::singleShot(0, this, SLOT(setUp()));
    }

    void HelperApp::setUp() {
        QStringList args = QCoreApplication::arguments();
        QString server;
        int pos;

        if ((pos = args.indexOf(QStringLiteral("--socket"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(AuthEnums::HELPER_OTHER_ERROR);
                return;
            }
            server = args[pos + 1];
        }

        if ((pos = args.indexOf(QStringLiteral("--id"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(AuthEnums::HELPER_OTHER_ERROR);
                return;
            }
            m_id = QString(args[pos + 1]).toLongLong();
        }

        if ((pos = args.indexOf(QStringLiteral("--start"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(AuthEnums::HELPER_OTHER_ERROR);
                return;
            }
            m_session->setPath(args[pos + 1]);
        }

        if ((pos = args.indexOf(QStringLiteral("--user"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(AuthEnums::HELPER_OTHER_ERROR);
                return;
            }
            m_user = args[pos + 1];
        }

        if ((pos = args.indexOf(QStringLiteral("--display-server"))) >= 0) {
            if (pos >= args.length() - 1) {
                qCritical() << "This application is not supposed to be executed manually";
                exit(AuthEnums::HELPER_OTHER_ERROR);
                return;
            }
            m_session->setDisplayServerCommand(args[pos + 1]);
            m_backend->setDisplayServer(true);
        }

        if ((pos = args.indexOf(QStringLiteral("--autologin"))) >= 0) {
            m_backend->setAutologin(true);
        }

        if ((pos = args.indexOf(QStringLiteral("--greeter"))) >= 0) {
            m_backend->setGreeter(true);
        }

        if (server.isEmpty() || m_id <= 0) {
            qCritical() << "This application is not supposed to be executed manually";
            exit(AuthEnums::HELPER_OTHER_ERROR);
            return;
        }

        // Enable/Disable retry limit for pam_chauthtok (fails with PAM_MAXTRIES) in sddm.conf,
        // password retry setting is usualy too low and aborts our password change dialog early
        if(mainConfig.RetryLoop.get() == true) {
            qDebug() << "Disabled password module retry limit";
            m_backend->setRetryLoop(true);
        }

        // read locale settings from distro specific file (default /etc/locale.conf)
        // and set (system) locale for pam conversations, i.e. LANG and LC_
        QProcessEnvironment env;
        Utils::readLocaleFile(env, mainConfig.LocaleFile.get());
        Utils::setLocaleEnv(env);

        // enable localization for (pam) backend,
        // set LC_ALL="" according to man page, otherwise C locale is selected
        setlocale (LC_ALL, "");

        connect(m_socket, &QLocalSocket::connected, this, &HelperApp::doAuth);
        connect(m_session, &UserSession::finished, this, &HelperApp::sessionFinished);
        m_socket->connectToServer(server, QIODevice::ReadWrite | QIODevice::Unbuffered);
    }

    void HelperApp::doAuth() {
        SafeDataStream str(m_socket);
        str << Msg::HELLO << m_id;
        str.send();
        if (str.status() != QDataStream::Ok)
            qCritical() << "Couldn't write initial message:" << str.status();

        if (!m_backend->start(m_user)) {
            authenticated(QString());
            exit(AuthEnums::HELPER_AUTH_ERROR);
            return;
        }

        if (!m_backend->authenticate()) {
            authenticated(QString());
            exit(AuthEnums::HELPER_AUTH_ERROR);
            return;
        }

        m_user = m_backend->userName();
        QProcessEnvironment env = authenticated(m_user);

        if (env.value(QStringLiteral("XDG_SESSION_CLASS")) == QLatin1String("greeter")) {
            for (const auto &entry : mainConfig.GreeterEnvironment.get()) {
                const int index = entry.indexOf(QLatin1Char('='));
                if (index < 0) {
                    qWarning() << "Malformed environment variable" << entry;
                    continue;
                }
                env.insert(entry.left(index), entry.mid(index + 1));
            }
        }

        if (!m_session->path().isEmpty()) {
            env.insert(m_session->processEnvironment());
            m_session->setProcessEnvironment(env);

            if (!m_backend->openSession()) {
                sessionOpened(false, -1);
                exit(AuthEnums::HELPER_SESSION_ERROR);
                return;
            }

            sessionOpened(true, m_session->processId());
        }
        else
            exit(AuthEnums::HELPER_SUCCESS);
        return;
    }

    void HelperApp::sessionFinished(int status) {
        m_backend->closeSession();

        exit(status);
    }

    void HelperApp::info(const QString& message, AuthEnums::Info type, int result) {
        SafeDataStream str(m_socket);
        str << Msg::INFO << message << type << result;
        str.send();
        m_socket->waitForBytesWritten();
    }

    void HelperApp::error(const QString& message, AuthEnums::Error type, int result) {
        SafeDataStream str(m_socket);
        str << Msg::ERROR << message << type << result;
        str.send();
        m_socket->waitForBytesWritten();
    }

    Request HelperApp::request(const Request& request, bool &cancel) {
        SafeDataStream str(m_socket);
        Msg m = Msg::MSG_UNKNOWN;
        Request response;
        str << Msg::REQUEST << request;
        str.send();
        m_socket->waitForBytesWritten();
        str.receive();
        str >> m;
        switch(m) {
            // user response from daemon (greeter)
            case REQUEST:
                str >> response;
                qDebug() << "HelperApp: daemon response received";
                break;
            // password change canceled in greeter
            case CANCEL:
                cancel = true;
                qDebug() << "HelperApp: Message received from daemon: CANCEL";
                // terminate user session in Auth (QProcess child)
                m_session->terminate();
                break;
            default:
                response = Request();
                qCritical() << "HelperApp: Received a wrong opcode instead of REQUEST or CANCEL:" << m;
        } // switch
        return response;
    }

    QProcessEnvironment HelperApp::authenticated(const QString &user) {
        Msg m = Msg::MSG_UNKNOWN;
        QProcessEnvironment env;
        SafeDataStream str(m_socket);
        str << Msg::AUTHENTICATED << user;
        str.send();
        m_socket->waitForBytesWritten();
        if (user.isEmpty())
            return env;
        str.receive();
        str >> m >> env >> m_cookie;
        if (m != AUTHENTICATED) {
            env = QProcessEnvironment();
            m_cookie = QString();
            qCritical() << "Received a wrong opcode instead of AUTHENTICATED:" << m;
        }
        return env;
    }

    void HelperApp::sessionOpened(bool success, qint64 pid) {
        Msg m = Msg::MSG_UNKNOWN;
        SafeDataStream str(m_socket);
        str << Msg::SESSION_STATUS << success << pid;
        str.send();
        m_socket->waitForBytesWritten();
        str.receive();
        str >> m;
        if (m != SESSION_STATUS) {
            qCritical() << "Received a wrong opcode instead of SESSION_STATUS:" << m;
        }
    }

    void HelperApp::displayServerStarted(const QString &displayName)
    {
        Msg m = Msg::MSG_UNKNOWN;
        SafeDataStream str(m_socket);
        str << Msg::DISPLAY_SERVER_STARTED << displayName;
        str.send();
        str.receive();
        str >> m;
        if (m != DISPLAY_SERVER_STARTED) {
            qCritical() << "Received a wrong opcode instead of DISPLAY_SERVER_STARTED:" << m;
        }
    }

    UserSession *HelperApp::session() {
        return m_session;
    }

    const QString& HelperApp::user() const {
        return m_user;
    }

    const QString& HelperApp::cookie() const {
        return m_cookie;
    }

    HelperApp::~HelperApp() {

    }
}

int main(int argc, char** argv) {

    SDDM::HelperApp app(argc, argv);
    return app.exec();
}
