/***************************************************************************
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
#include "DisplayServer.h"
#include "Seat.h"
#include "SocketServer.h"
#include "Greeter.h"
#include <pwd.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTimer>

namespace SDDM {
    QString generateName(int length) {
        QString digits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        // reserve space for name
        QString name;
        name.reserve(length);

        // create random device
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, digits.length() - 1);

        // generate name
        for (int i = 0; i < length; ++i)
            name[i] = digits.at(dis(gen));

        // return result
        return name;
    }

    Display::Display(const int displayId, const int terminalId, Seat *parent) : QObject(parent),
        m_displayId(displayId), m_terminalId(terminalId),
        m_displayServer(new DisplayServer(this)),
        m_seat(parent),
        m_auth(new QAuth(this)),
        m_socketServer(new SocketServer(this)),
        m_greeter(new Greeter(this)) {

        m_display = QString(":%1").arg(m_displayId);

        m_auth->setVerbose(true);
        connect(m_auth, SIGNAL(requestChanged()), this, SLOT(slotRequestChanged()));
        connect(m_auth, SIGNAL(authentication(QString,bool)), this, SLOT(slotAuthenticationFinished(QString,bool)));
        connect(m_auth, SIGNAL(session(bool)), this, SLOT(slotSessionStarted(bool)));
        connect(m_auth, SIGNAL(finished(bool)), this, SLOT(slotHelperFinished(bool)));
        connect(m_auth, SIGNAL(error(QString)), this, SLOT(slotAuthError(QString)));

        // restart display after display server ended
        connect(m_displayServer, SIGNAL(stopped()), this, SLOT(stop()));

        // connect login signal
        connect(m_socketServer, SIGNAL(login(QLocalSocket*,QString,QString,QString)), this, SLOT(login(QLocalSocket*,QString,QString,QString)));

        // connect login result signals
        connect(this, SIGNAL(loginFailed(QLocalSocket*)), m_socketServer, SLOT(loginFailed(QLocalSocket*)));
        connect(this, SIGNAL(loginSucceeded(QLocalSocket*)), m_socketServer, SLOT(loginSucceeded(QLocalSocket*)));

        // get auth dir
        QString authDir = daemonApp->configuration()->authDir();

        // use "." as authdir in test mode
        if (daemonApp->configuration()->testing)
            authDir = QLatin1String(".");

        // create auth dir if not existing
        QDir().mkpath(authDir);

        // set auth path
        m_authPath = QString("%1/A%2-%3").arg(authDir).arg(m_display).arg(generateName(6));

        // set socket name
        m_socketName = QString("sddm-%1-%2").arg(m_display).arg(generateName(6));
    }

    Display::~Display() {
        stop();
    }

    const int Display::displayId() const {
        return m_displayId;
    }

    const int Display::terminalId() const {
        return m_terminalId;
    }

    const QString &Display::name() const {
        return m_display;
    }

    const QString &Display::cookie() const {
        return m_cookie;
    }

    Seat *Display::seat() const {
        return m_seat;
    }

    void Display::addCookie(const QString &file) {
        // log message
        qDebug() << " DAEMON: Adding cookie to" << file;

        // Touch file
        QFile file_handler(file);
        file_handler.open(QIODevice::WriteOnly);
        file_handler.close();

        QString cmd = QString("%1 -f %2 -q").arg(daemonApp->configuration()->xauthPath()).arg(file);

        // execute xauth
        FILE *fp = popen(qPrintable(cmd), "w");

        // check file
        if (!fp)
            return;
        fprintf(fp, "remove %s\n", qPrintable(m_display));
        fprintf(fp, "add %s . %s\n", qPrintable(m_display), qPrintable(m_cookie));
        fprintf(fp, "exit\n");
        // close pipe
        pclose(fp);
    }

    void Display::start() {
        // check flag
        if (m_started)
            return;

        // generate cookie
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        // resever 32 bytes
        m_cookie.reserve(32);

        // create a random hexadecimal number
        const char *digits = "0123456789abcdef";
        for (int i = 0; i < 32; ++i)
            m_cookie[i] = digits[dis(gen)];

        // generate auth file
        addCookie(m_authPath);

        // set display server params
        m_displayServer->setDisplay(m_display);
        m_displayServer->setAuthPath(m_authPath);

        // start display server
        m_displayServer->start();

        if ((daemonApp->configuration()->first || daemonApp->configuration()->autoRelogin()) &&
            !daemonApp->configuration()->autoUser().isEmpty() && !daemonApp->configuration()->lastSession().isEmpty()) {
            // reset first flag
            daemonApp->configuration()->first = false;

            // set flags
            m_started = true;

            // start session
            m_auth->setAutologin(true);
            startAuth(daemonApp->configuration()->autoUser(), QString(), daemonApp->configuration()->lastSession());

            // return
            return;
        }

        // set socket server name
        m_socketServer->setSocket(m_socketName);

        // start socket server
        m_socketServer->start();

        // set greeter params
        m_greeter->setDisplay(m_display);
        m_greeter->setAuthPath(m_authPath);
        m_greeter->setSocket(m_socketName);
        m_greeter->setTheme(QString("%1/%2").arg(daemonApp->configuration()->themesDir()).arg(daemonApp->configuration()->currentTheme()));

        // start greeter
        m_greeter->start();

        // reset first flag
        daemonApp->configuration()->first = false;

        // set flags
        m_started = true;
    }

    void Display::stop() {
        // check flag
        if (!m_started)
            return;

        // stop user session
        /*
        m_authenticator->blockSignals(true);
        m_authenticator->stop();
        m_authenticator->blockSignals(false);
        */

        // stop the greeter
        m_greeter->stop();

        // stop socket server
        m_socketServer->stop();

        // stop display server
        m_displayServer->blockSignals(true);
        m_displayServer->stop();
        m_displayServer->blockSignals(false);

        // remove authority file
        QFile::remove(m_authPath);

        // reset flag
        m_started = false;

        // emit signal
        emit stopped();
    }

    void Display::login(QLocalSocket *socket, const QString &user, const QString &password, const QString &session) {
        // start session
        m_socket = socket;
        startAuth(user, password, session);
    }

    void Display::startAuth(const QString& user, const QString& password, const QString& session) {
        QString sessionName;
        QString xdgCurrentDesktop;

        m_passPhrase = password;

        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(daemonApp->configuration()->sessionsDir());

            // session file
            QFile file(dir.absoluteFilePath(session));

            // open file
            if (file.open(QIODevice::ReadOnly)) {

                // read line-by-line
                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine();

                    // line starting with Exec
                    if (line.startsWith("Exec="))
                        m_auth->setSession(line.mid(5));

                    // Desktop names, change the separator
                    if (line.startsWith("DesktopNames=")) {
                        xdgCurrentDesktop = line.mid(13);
                        xdgCurrentDesktop.replace(';', ':');
                    }
                }

                // close file
                file.close();
            }

            // remove extension
            sessionName = session.left(session.lastIndexOf("."));
        } else {
            m_auth->setSession(session);
            sessionName = session;
        }

        QProcessEnvironment env;
        env.insert("PATH", daemonApp->configuration()->defaultPath());
        env.insert("DISPLAY", name());
        env.insert("XDG_SEAT", seat()->name());
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat()->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(QString("Session%1").arg(daemonApp->newSessionId())));
        env.insert("XDG_VTNR", QString::number(terminalId()));
        env.insert("DESKTOP_SESSION", sessionName);
        env.insert("XDG_CURRENT_DESKTOP", xdgCurrentDesktop);
        m_auth->insertEnvironment(env);

        m_auth->setUser(user);
        m_auth->start();
    }

    void Display::slotAuthenticationFinished(QString user, bool success) {
        if (success) {
            qDebug() << " DAEMON: Authenticated successfully";
            struct passwd *pw = getpwnam(qPrintable(user));
            if (pw) {
                addCookie(QString("%1/.Xauthority").arg(pw->pw_dir));
                chown(qPrintable(QString("%1/.Xauthority").arg(pw->pw_dir)), pw->pw_uid, pw->pw_gid);
            }
            daemonApp->configuration()->setLastUser(m_auth->user());
            daemonApp->configuration()->setLastSession(m_auth->session());
//             FIXME: nukes the config, what the hell
//             daemonApp->configuration()->save();
            if (m_socket)
                emit loginSucceeded(m_socket);
        }
        else if (m_socket) {
            qDebug() << " DAEMON: Authentication failure";
            emit loginFailed(m_socket);
        }
        m_socket = nullptr;
    }

    // presentable to the user, eventually
    void Display::slotAuthError(QString message) {
        qWarning() << " DAEMON: Auth:" << message;
    }

    void Display::slotHelperFinished(bool success) {
        stop();
    }

    void Display::slotRequestChanged() {
        if (m_auth->request()->prompts().length() == 1) {
            m_auth->request()->prompts()[0]->setResponse(qPrintable(m_passPhrase));
            m_auth->request()->done();
        }
        else if (m_auth->request()->prompts().length() == 2) {
            m_auth->request()->prompts()[0]->setResponse(qPrintable(m_auth->user()));
            m_auth->request()->prompts()[1]->setResponse(qPrintable(m_passPhrase));
            m_auth->request()->done();
        }
    }

    void Display::slotSessionStarted(bool success) {
        qDebug() << " DAEMON: Session started";
    }

}
