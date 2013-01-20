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

#include "Configuration.h"

#include <QSettings>

namespace SDE {
    static Configuration *_instance = nullptr;

    class ConfigurationPrivate {
    public:
        QString configPath { "" };

        QString defaultPath { "" };

        QString serverPath { "" };
        QStringList serverArgs;

        QString xauthPath { "" };

        QString authFile { "" };
        QString lockFile { "" };

        QString haltCommand { "" };
        QString rebootCommand { "" };

        QString sessionsDir { "" };
        QString lastSession { "" };
        QString sessionCommand { "" };

        QString themesDir { "" };
        QString currentTheme { "" };

        QString lastUser { "" };
    };

    Configuration::Configuration(const QString &configPath) : d(new ConfigurationPrivate()) {
        _instance = this;
        // set config path
        d->configPath = configPath;
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // read settings
        d->defaultPath = settings.value("DefaultPath", "").toString();
        d->serverPath = settings.value("ServerPath", "").toString();
        d->serverArgs = settings.value("ServerArgs", "").toString().split(" ");
        d->xauthPath = settings.value("XauthPath", "").toString();
        d->authFile = settings.value("AuthFile", "").toString();
        d->lockFile = settings.value("LockFile", "").toString();
        d->haltCommand = settings.value("HaltCommand", "").toString();
        d->rebootCommand = settings.value("RebootCommand", "").toString();
        d->sessionsDir = settings.value("SessionsDir", "").toString();
        d->lastSession = settings.value("LastSession", "").toString();
        d->sessionCommand = settings.value("SessionCommand", "").toString();
        d->themesDir = settings.value("ThemesDir", "").toString();
        d->currentTheme = settings.value("CurrentTheme", "").toString();
        d->lastUser = settings.value("LastUser", "").toString();
    }

    Configuration::~Configuration() {
        // save settings
        save();
        // clean up
        delete d;
    }

    void Configuration::save() {
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // write settings back
        settings.setValue("DefaultPath", d->defaultPath);
        settings.setValue("ServerPath", d->serverPath);
        settings.setValue("ServerArgs", d->serverArgs.join(" "));
        settings.setValue("XauthPath", d->xauthPath);
        settings.setValue("AuthFile", d->authFile);
        settings.setValue("LockFile", d->lockFile);
        settings.setValue("HaltCommand", d->haltCommand);
        settings.setValue("RebootCommand", d->rebootCommand);
        settings.setValue("SessionsDir", d->sessionsDir);
        settings.setValue("LastSession", d->lastSession);
        settings.setValue("SessionCommand", d->sessionCommand);
        settings.setValue("ThemesDir", d->themesDir);
        settings.setValue("CurrentTheme", d->currentTheme);
        settings.setValue("LastUser", d->lastUser);
    }

    Configuration *Configuration::instance() {
        return _instance;
    }

    const QString &Configuration::defaultPath() const {
        return d->defaultPath;
    }

    const QString &Configuration::serverPath() const {
        return d->serverPath;
    }

    const QStringList &Configuration::serverArgs() const {
        return d->serverArgs;
    }

    const QString &Configuration::xauthPath() const {
        return d->xauthPath;
    }

    const QString &Configuration::authFile() const {
        return d->authFile;
    }

    const QString &Configuration::lockFile() const {
        return d->lockFile;
    }

    const QString &Configuration::haltCommand() const {
        return d->haltCommand;
    }

    const QString &Configuration::rebootCommand() const {
        return d->rebootCommand;
    }

    const QString &Configuration::sessionsDir() const {
        return d->sessionsDir;
    }

    const QString &Configuration::lastSession() const {
        return d->lastSession;
    }

    const QString &Configuration::sessionCommand() const {
        return d->sessionCommand;
    }

    void Configuration::setLastSession(const QString &lastSession) {
        d->lastSession = lastSession;
    }

    const QString &Configuration::themesDir() const {
        return d->themesDir;
    }

    const QString &Configuration::currentTheme() const {
        return d->currentTheme;
    }

    const QString &Configuration::lastUser() const {
        return d->lastUser;
    }

    void Configuration::setLastUser(const QString &lastUser) {
        d->lastUser = lastUser;
    }
}
