/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com
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

#include "SessionManager.h"

#include "Authenticator.h"
#include "Configuration.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <unistd.h>

using namespace SDE;
namespace SDE {
    class SessionInfo {
    public:
        QString file;
        QString name;
        QString exec;
        QString comment;
    };

    class SessionManagerPrivate {
    public:
        Authenticator *authenticator { new Authenticator("sddm") };

        std::vector<SessionInfo> sessions;
        int lastSession { 0 };
        QStringList sessionList;
        QString hostName { "" };
    };

    SessionManager::SessionManager() : d(new SessionManagerPrivate()) {
        // read session files
        QDir dir(Configuration::instance()->sessionsDir());
        int i = 0;
        foreach (const QString &session, dir.entryList()) {
            if (!session.endsWith(".desktop"))
                continue;
            QFile inputFile(dir.absoluteFilePath(session));
            if (inputFile.open(QIODevice::ReadOnly)) {
                QTextStream in(&inputFile);
                SessionInfo si;
                si.file = session;
                while (!in.atEnd()) {
                    QString line = in.readLine();
                    if (line.startsWith("Name="))
                        si.name = line.mid(5);
                    if (line.startsWith("Exec="))
                        si.exec = line.mid(5);
                    if (line.startsWith("Comment="))
                        si.comment = line.mid(8);
                }
                d->sessions.push_back(si);
                d->sessionList << si.name;
                // check last session index
                if (session == Configuration::instance()->lastSession())
                    d->lastSession = i;
                i++;
            }
        }
        // get hostname
        char hostName[100];
        gethostname(hostName, 100);
        d->hostName = hostName;
    }

    SessionManager::~SessionManager() {
        delete d->authenticator;
        delete d;
    }

    const int SessionManager::lastSession() const {
        return d->lastSession;
    }

    const QStringList &SessionManager::sessions() const {
        return d->sessionList;
    }

    const QString &SessionManager::hostName() const {
        return d->hostName;
    }

    const QString &SessionManager::lastUser() const {
        return Configuration::instance()->lastUser();
    }

    void SessionManager::setCookie(const Cookie &cookie) {
        d->authenticator->setCookie(cookie);
    }

    void SessionManager::setDisplay(const QString &displayName) {
        d->authenticator->setDisplay(displayName);
    }

    void SessionManager::autoLogin() {
        // set user name
        d->authenticator->setUsername(Configuration::instance()->autoUser());
        // login without authenticating
        d->authenticator->login(d->sessions[d->lastSession].exec);
    }

    void SessionManager::login(const QString &username, const QString &password, const int sessionIndex) {
        // set user name and password
        d->authenticator->setUsername(username);
        d->authenticator->setPassword(password);
        // try to authenticate
        if (!d->authenticator->authenticate()) {
            // emit login fail signal
            emit fail();
            // return
            return;
        }
        // emit login success signal
        emit success();
        // save last session and last user
        Configuration::instance()->setLastSession(d->sessions[sessionIndex].file);
        Configuration::instance()->setLastUser(username);
        Configuration::instance()->save();
        // login
        d->authenticator->login(d->sessions[sessionIndex].exec);
        // quit application
        qApp->quit();
    }

    void SessionManager::shutdown() {
        system(Configuration::instance()->haltCommand().toStdString().c_str());
    }

    void SessionManager::reboot() {
        system(Configuration::instance()->rebootCommand().toStdString().c_str());
    }
}
