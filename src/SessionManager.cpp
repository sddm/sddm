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
#include <QHostInfo>
#include <QProcess>
#include <QTextStream>

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
        Authenticator authenticator { "sddm" };

        std::vector<SessionInfo> sessions;
        int lastSession { 0 };
        QStringList sessionList;
        QString hostName { "" };
    };

    SessionManager::SessionManager() : d(new SessionManagerPrivate()) {
        // add custom and failsafe session
        d->sessions.push_back({ "custom", "Custom", "custom", "Custom Session" });
        d->sessions.push_back({ "failsafe", "Failsafe", "failsafe", "Failsafe Session" });
        // read session files
        QDir dir(Configuration::instance()->sessionsDir());
        dir.setNameFilters(QStringList() << "*.desktop");
        dir.setFilter(QDir::Files);
        // read session
        foreach (const QString &session, dir.entryList()) {
            QFile inputFile(dir.absoluteFilePath(session));
            if (!inputFile.open(QIODevice::ReadOnly))
                continue;
            SessionInfo si { session, "", "", "" };
            QTextStream in(&inputFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("Name="))
                    si.name = line.mid(5);
                if (line.startsWith("Exec="))
                    si.exec = line.mid(5);
                if (line.startsWith("Comment="))
                    si.comment = line.mid(8);
            }
            // add to sessions list
            d->sessions.push_back(si);
            // close file
            inputFile.close();
        }
        // check last session index
        d->lastSession = 0;
        for (int i = 0; i < d->sessions.size(); ++i) {
            d->sessionList << d->sessions.at(i).name;
            if (d->sessions.at(i).file == Configuration::instance()->lastSession())
                d->lastSession = i;
        }
        // get hostname
        d->hostName = QHostInfo::localHostName();
    }

    SessionManager::~SessionManager() {
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
        d->authenticator.setCookie(cookie);
    }

    void SessionManager::setDisplay(const QString &displayName) {
        d->authenticator.setDisplay(displayName);
    }

    void SessionManager::autoLogin() {
        // set user name
        d->authenticator.setUsername(Configuration::instance()->autoUser());
        // login without authenticating
        if ((d->lastSession >= 0) && (d->lastSession < d->sessions.size()))
            d->authenticator.login(d->sessions[d->lastSession].exec);
    }

    void SessionManager::login(const QString &username, const QString &password, const int sessionIndex) {
        // set user name and password
        d->authenticator.setUsername(username);
        d->authenticator.setPassword(password);
        // try to authenticate
        if (!d->authenticator.authenticate()) {
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
        if ((sessionIndex >= 0) && (sessionIndex < d->sessions.size()))
            d->authenticator.login(d->sessions[sessionIndex].exec);
        // quit application
        qApp->quit();
    }

    void SessionManager::shutdown() {
        QProcess::execute(Configuration::instance()->haltCommand());
    }

    void SessionManager::reboot() {
        QProcess::execute(Configuration::instance()->rebootCommand());
    }
}
