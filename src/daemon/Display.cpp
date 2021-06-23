/***************************************************************************
* Copyright (c) 2018 Thomas Höhn <thomas_hoehn@gmx.net>
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
#include "SignalHandler.h"
#include "Utils.h"

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QLocalSocket>
#include <QByteArray>

#include <pwd.h>
#include <unistd.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include "Login1Manager.h"
#include "Login1Session.h"
#include "VirtualTerminal.h"
#include "WaylandDisplayServer.h"

#if defined(Q_OS_LINUX)
#include <utmp.h>
#endif
#include <utmpx.h>

namespace SDDM {
    Display::Display(Seat *parent) : QObject(parent),
        m_auth(new Auth(this)),
        m_seat(parent),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this)) {

        // Allocate vt
        m_terminalId = VirtualTerminal::setUpNewVt();

        // Save display server type
        const QString &displayServerType = mainConfig.DisplayServer.get().toLower();
        if (displayServerType == QLatin1String("x11"))
            m_displayServerType = X11DisplayServerType;
        else if (displayServerType == QStringLiteral("x11-user"))
            m_displayServerType = X11UserDisplayServerType;
        else if (displayServerType == QStringLiteral("wayland"))
            m_displayServerType = WaylandDisplayServerType;
        else {
            qWarning("\"%s\" is an invalid value for General.DisplayServer: fall back to \"x11\"",
                     qPrintable(displayServerType));
            m_displayServerType = X11DisplayServerType;
        }

        // Create display server
        switch (m_displayServerType) {
        case X11DisplayServerType:
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

        // Print what VT we are using for more information
        qDebug("Using VT %d", m_terminalId);

        connect(m_greeter, SIGNAL(stopped()), this, SLOT(slotGreeterStopped()));

        // respond to authentication requests
        m_auth->setVerbose(true);
        connect(m_auth, &Auth::requestChanged, this, &Display::slotRequestChanged);
        connect(m_auth, &Auth::authentication, this, &Display::slotAuthenticationFinished);
        connect(m_auth, &Auth::sessionStarted, this, &Display::slotSessionStarted);
        connect(m_auth, &Auth::finished, this, &Display::slotHelperFinished);
        connect(m_auth, &Auth::info, this, &Display::slotAuthInfo);
        connect(m_auth, &Auth::error, this, &Display::slotAuthError);

        // restart display after display server ended
        connect(m_displayServer, &DisplayServer::started, this, &Display::displayServerStarted);
        connect(m_displayServer, &DisplayServer::stopped, this, &Display::stop);

        // connect login signal
        connect(m_socketServer, &SocketServer::login, this, &Display::login);
        // pam responses and conversation cancel
        connect(m_socketServer, &SocketServer::sendPamResponse, this, &Display::setPamResponse);
        connect(m_socketServer, &SocketServer::cancelPamConv, this, &Display::cancelPamConv);

        // connect login result signals
        connect(this, &Display::loginFailed, m_socketServer, &SocketServer::loginFailed);
        connect(this, &Display::loginSucceeded, m_socketServer, &SocketServer::loginSucceeded);
        // pam (conversation) info/error shown in greeter
        connect(this, &Display::pamConvMsg, m_socketServer, &SocketServer::pamConvMsg);
        // new request from pam for e.g. password renewal (expired password)
        connect(this, &Display::pamRequest, m_socketServer, &SocketServer::pamRequest);
    }

    Display::~Display() {
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

    QString Display::displayId() const {
        return m_displayServer->display();
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

    bool Display::start() {
        return m_started || m_displayServer->start();
    }

    bool Display::attemptAutologin() {
        Session::Type sessionType = Session::X11Session;

        // determine session type
        QString autologinSession = mainConfig.Autologin.Session.get();
        // not configured: try last successful logged in
        if (autologinSession.isEmpty()) {
            autologinSession = stateConfig.Last.Session.get();
        }
        if (findSessionEntry(mainConfig.Wayland.SessionDir.get(), autologinSession)) {
            sessionType = Session::WaylandSession;
        } else if (findSessionEntry(mainConfig.X11.SessionDir.get(), autologinSession)) {
            sessionType = Session::X11Session;
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

        // setup display
        m_displayServer->setupDisplay();

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

        // set greeter params
        m_greeter->setDisplay(this);
        if (qobject_cast<XorgDisplayServer *>(m_displayServer))
            m_greeter->setAuthPath(qobject_cast<XorgDisplayServer *>(m_displayServer)->authPath());
        m_greeter->setSocket(m_socketServer->socketAddress());
        m_greeter->setTheme(findGreeterTheme());

        // start greeter
        m_greeter->start();

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
        m_greeter->stop();

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

        m_socket = socket;
        m_failed = false;

        //the SDDM user has special privileges that skip password checking so that we can load the greeter
        //block ever trying to log in as the SDDM user
        if (user == QLatin1String("sddm")) {
            return;
        }

        // authenticate
        startAuth(user, password, session);
    }

    // password change, got response from greeter
    void Display::setPamResponse(const QString &password) {
        qDebug() << "Display: set pam response with new password";
        m_auth->request()->setChangeResponse(password);
        if(m_auth->request()->finishAutomatically() ==  false)
            m_auth->request()->done();
    }

    // cancel pam (password change) conversation
    // because user canceled password dialog in greeter
    void Display::cancelPamConv() {
        m_auth->request()->cancel();
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

    bool Display::findSessionEntry(const QDir &dir, const QString &name) const {
        // Given an absolute path: Check that it matches dir
        const QFileInfo fileInfo(name);
        if (fileInfo.isAbsolute() && fileInfo.absolutePath() != dir.absolutePath())
            return false;

        QString fileName = name;

        // append extension
        const QString extension = QStringLiteral(".desktop");
        if (!fileName.endsWith(extension))
            fileName += extension;

        return dir.exists(fileName);
    }

    void Display::startAuth(const QString &user, const QString &password, const Session &session) {

        if (m_auth->isActive()) {
            qWarning() << "Existing authentication ongoing, aborting";
            return;
        }

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

        m_reuseSessionId = QString();

        if (Logind::isAvailable() && mainConfig.Users.ReuseSession.get()) {
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
            auto reply = manager.ListSessions();
            reply.waitForFinished();

            const auto info = reply.value();
            for(const SessionInfo &s : reply.value()) {
                if (s.userName == user) {
                    OrgFreedesktopLogin1SessionInterface session(Logind::serviceName(), s.sessionPath.path(), QDBusConnection::systemBus());
                    if (session.service() == QLatin1String("sddm") && session.state() == QLatin1String("online")) {
                        m_reuseSessionId = s.sessionId;
                        break;
                    }
                }
            }
        }

        // cache last session
        m_lastSession = session;

        // save session desktop file name, we'll use it to set the
        // last session later, in slotAuthenticationFinished()
        m_sessionName = session.fileName();

        // New VT
        m_lastSession.setVt(VirtualTerminal::setUpNewVt());

        // some information
        qDebug() << "Session" << m_sessionName << "selected, command:" << session.exec();

        QProcessEnvironment env;
        env.insert(session.additionalEnv());

        env.insert(QStringLiteral("PATH"), mainConfig.Users.DefaultPath.get());
        env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
        env.insert(QStringLiteral("DESKTOP_SESSION"), session.desktopSession());
        env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), session.desktopNames());
        env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("user"));
        env.insert(QStringLiteral("XDG_SESSION_TYPE"), session.xdgSessionType());
        env.insert(QStringLiteral("XDG_SEAT"), seat()->name());
        env.insert(QStringLiteral("XDG_VTNR"), QString::number(m_lastSession.vt()));
        env.insert(QStringLiteral("XDG_SESSION_DESKTOP"), session.desktopNames());

        m_auth->insertEnvironment(env);

        if (session.xdgSessionType() == QLatin1String("x11"))
            m_auth->setDisplayServerCommand(XorgUserDisplayServer::command(this));
        else
            m_auth->setDisplayServerCommand(QStringLiteral());

        m_auth->setUser(user);
        if (m_reuseSessionId.isNull()) {
            m_auth->setSession(session.exec());
        }
        m_auth->start();
    }

    void Display::slotAuthenticationFinished(const QString &user, bool success) {
        if (success) {
            qDebug() << "Authenticated successfully" << user;

            if (!m_reuseSessionId.isNull()) {
                OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(), Logind::managerPath(), QDBusConnection::systemBus());
                manager.UnlockSession(m_reuseSessionId);
                manager.ActivateSession(m_reuseSessionId);
            } else {
                if (qobject_cast<XorgDisplayServer *>(m_displayServer))
                    m_auth->setCookie(qobject_cast<XorgDisplayServer *>(m_displayServer)->cookie());
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
            utmpLogin(QString::number(terminalId()),  name(), user, 0, false);
            qDebug() << "Authentication failure";

            if (!m_socket) {
                qWarning() << "Greeter socket down!";
                return;
            }

            // avoid to emit loginFailed twice
            if(!m_failed) {
                emit loginFailed(m_socket, QString(),0);
                m_failed = true;
            }
        }
        m_socket = nullptr;
    }

    // presentable to the user
    void Display::slotAuthInfo(const QString &message, AuthEnums::Info info, int result) {

        qWarning().noquote().nospace() << "Authentication information: message = \"" << message
                                       << "\", type = " << Utils::authInfoString(info)
                                       << ", result = " << Utils::pamResultString(result)
                                       << " (rc=" << result << ")";

        if(!m_socket) {
            qWarning() << "Greeter socket down";
            return;
        }

        if(info == AuthEnums::INFO_PAM_CONV)
        {
            // send pam conversation message to greeter
            emit pamConvMsg(m_socket, message, result);
        }
    }

    void Display::slotAuthError(const QString &message, AuthEnums::Error error, int result) {

        qWarning().noquote().nospace() << "Authentication error: message = \"" << message
                                       << "\", type = " << Utils::authErrorString(error)
                                       << ", result = " << Utils::pamResultString(result)
                                       << " (rc=" << result << ")";

        if (error == AuthEnums::ERROR_AUTHENTICATION)
            m_failed = true;

        if (!m_socket) {
            qWarning() << "Greeter socket down!";
            return;
        }

        // failed login only when result not PAM_SUCCESS,
        // i.e. error is of type ERROR_AUTHENTICATION
        if (error == AuthEnums::ERROR_AUTHENTICATION)
            emit loginFailed(m_socket, message, result);
        else if(error == AuthEnums::ERROR_PAM_CONV)
                emit pamConvMsg(m_socket, message, result);

        // ignore internal errors, no emit
    }

    void Display::slotHelperFinished(AuthEnums::HelperExitStatus status) {
        if (m_auth->sessionPid() > 0) {
            utmpLogout(QString::number(terminalId()), name(), m_auth->sessionPid());
        }

        // Don't restart greeter and display server unless sddm-helper exited
        // with an internal error or the user session finished successfully,
        // we want to avoid greeter from restarting when an authentication
        // error happens (in this case we want to show the message from the
        // greeter
        if (status != AuthEnums::HELPER_AUTH_ERROR)
            stop();

        // Start the greeter again as soon as the user session is closed
        if (m_auth->user() != QLatin1String("sddm"))
            m_greeter->start();
    }

    /* got new request (with prompts) from pam conv(), e.g. for expired pwd
     * which requires response from greeter UI (new password) */
    void Display::slotRequestChanged() {

        int n_prompts = m_auth->request()->prompts().count();

        // ignore empty requests
        if(n_prompts<=0) return;

        // see what we got from pam conv() and will be send to greeter
        qDebug() << "Display: requestChanged with " << n_prompts << " prompts from Auth";

        // handle password change case (gets response from greeter),
        // will finish with request->done() later in setPamResponse()
        if (m_auth->request()->findPrompt(AuthPrompt::CHANGE_PASSWORD))
        {
            if(m_socket)
                // send password change request to greeter (via SocketServer)
                emit pamRequest(m_socket, m_auth->request());
            return;
        }

        // handle requests with user name and password prompts, respond to login password request
        if ((m_auth->request()->findPrompt(AuthPrompt::LOGIN_USER) ||
             m_auth->request()->findPrompt(AuthPrompt::LOGIN_PASSWORD)) &&
             m_auth->request()->setLoginResponse(m_auth->user(), m_passPhrase) == true)
        {
             m_auth->request()->done();
             return;
        }

        qWarning() << "Display: Unable to handle Auth request!";
        m_auth->request()->cancel();
    }

    void Display::slotSessionStarted(bool success, qint64 pid) {
        if (success) {
            utmpLogin(QString::number(terminalId()), name(), m_auth->user(), pid, true);
        }
    }

    void Display::utmpLogin(const QString &vt, const QString &displayName, const QString &user, qint64 pid, bool authSuccessful) {
        struct utmpx entry;
        struct timeval tv;

        entry = { 0 };
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

#if !defined(Q_OS_FREEBSD)
        // append to failed login database btmp
        if (!authSuccessful) {
#if defined(Q_OS_LINUX)
            updwtmpx("/var/log/btmp", &entry);
#endif
        }

        // append to wtmp
        else {
#if defined(Q_OS_LINUX)
            updwtmpx("/var/log/wtmp", &entry);
#endif
        }
#endif
    }

    void Display::utmpLogout(const QString &vt, const QString &displayName, qint64 pid) {
        struct utmpx entry;
        struct timeval tv;

        entry = { 0 };
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

    // inform daemon if greeter stopped, so we dont forward
    // errors or infos etc. through invalid socket
    void Display::slotGreeterStopped() {
        qDebug() << "Display: Greeter was stopped";
        m_socket = nullptr;
    }
}
