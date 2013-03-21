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

#include "Configuration.h"

#include <QSettings>

namespace SDDM {
    static Configuration *_instance = nullptr;

    class ConfigurationPrivate {
    public:
        QString configPath { "" };

        QString cursorTheme { "" };

        QString defaultPath { "" };

        QString serverPath { "" };

        QString xauthPath { "" };

        QString authDir { "" };

        QString haltCommand { "" };
        QString rebootCommand { "" };

        QString sessionsDir { "" };
        bool rememberLastSession { true };
        QString lastSession { "" };
        QString sessionCommand { "" };

        QString facesDir { "" };

        QString themesDir { "" };
        QString currentTheme { "" };

        int minimumUid { 0 };
        int maximumUid { 65000 };
        QStringList hideUsers;
        QStringList hideShells;

        bool rememberLastUser { true };
        QString lastUser { "" };

        QString autoUser { "" };
        bool autoRelogin { false };
    };

    Configuration::Configuration(const QString &configPath, QObject *parent) : QObject(parent), d(new ConfigurationPrivate()) {
        _instance = this;
        // set config path
        d->configPath = configPath;
        // load settings
        load();
    }

    Configuration::~Configuration() {
        // clean up
        delete d;
    }

    void Configuration::load() {
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // read settings
        d->cursorTheme = settings.value("CursorTheme", "").toString();
        d->defaultPath = settings.value("DefaultPath", "").toString();
        d->serverPath = settings.value("ServerPath", "").toString();
        d->xauthPath = settings.value("XauthPath", "").toString();
        d->authDir = settings.value("AuthDir", "").toString();
        d->haltCommand = settings.value("HaltCommand", "").toString();
        d->rebootCommand = settings.value("RebootCommand", "").toString();
        d->sessionsDir = settings.value("SessionsDir", "").toString();
        d->rememberLastSession = settings.value("RememberLastSession", d->rememberLastSession).toBool();
        d->lastSession = settings.value("LastSession", "").toString();
        d->sessionCommand = settings.value("SessionCommand", "").toString();
        d->facesDir = settings.value("FacesDir", "").toString();
        d->themesDir = settings.value("ThemesDir", "").toString();
        d->currentTheme = settings.value("CurrentTheme", "").toString();
        d->minimumUid = settings.value("MinimumUid", "0").toInt();
        d->maximumUid = settings.value("MaximumUid", "65000").toInt();
        d->hideUsers = settings.value("HideUsers", "").toString().split(' ', QString::SkipEmptyParts);
        d->hideShells = settings.value("HideShells", "").toString().split(' ', QString::SkipEmptyParts);
        d->rememberLastUser = settings.value("RememberLastUser", d->rememberLastUser).toBool();
        d->lastUser = settings.value("LastUser", "").toString();
        d->autoUser = settings.value("AutoUser", "").toString();
        d->autoRelogin = settings.value("AutoRelogin", d->autoRelogin).toBool();
    }

    void Configuration::save() {
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // write settings back
        settings.setValue("CursorTheme", d->cursorTheme);
        settings.setValue("DefaultPath", d->defaultPath);
        settings.setValue("ServerPath", d->serverPath);
        settings.setValue("XauthPath", d->xauthPath);
        settings.setValue("AuthDir", d->authDir);
        settings.setValue("HaltCommand", d->haltCommand);
        settings.setValue("RebootCommand", d->rebootCommand);
        settings.setValue("SessionsDir", d->sessionsDir);
        settings.setValue("RememberLastSession", d->rememberLastSession);
        settings.setValue("LastSession", d->lastSession);
        settings.setValue("SessionCommand", d->sessionCommand);
        settings.setValue("FacesDir", d->facesDir);
        settings.setValue("ThemesDir", d->themesDir);
        settings.setValue("CurrentTheme", d->currentTheme);
        settings.setValue("MinimumUid", d->minimumUid);
        settings.setValue("MaximumUid", d->maximumUid);
        settings.setValue("HideUsers", d->hideUsers.join(" "));
        settings.setValue("HideShells", d->hideShells.join(" "));
        settings.setValue("RememberLastUser", d->rememberLastUser);
        settings.setValue("LastUser", d->lastUser);
        settings.setValue("AutoUser", d->autoUser);
        settings.setValue("AutoRelogin", d->autoRelogin);
    }

    Configuration *Configuration::instance() {
        return _instance;
    }

    const QString &Configuration::cursorTheme() const {
        return d->cursorTheme;
    }

    const QString &Configuration::defaultPath() const {
        return d->defaultPath;
    }

    const QString &Configuration::serverPath() const {
        return d->serverPath;
    }

    const QString &Configuration::xauthPath() const {
        return d->xauthPath;
    }

    const QString &Configuration::authDir() const {
        return d->authDir;
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
        if (d->rememberLastSession)
            d->lastSession = lastSession;
    }

    const QString &Configuration::facesDir() const {
        return d->facesDir;
    }

    const QString &Configuration::themesDir() const {
        return d->themesDir;
    }

    const QString &Configuration::currentTheme() const {
        return d->currentTheme;
    }

    const int Configuration::minimumUid() const {
        return d->minimumUid;
    }

    const int Configuration::maximumUid() const {
        return d->maximumUid;
    }

    const QStringList &Configuration::hideUsers() const {
        return d->hideUsers;
    }

    const QStringList &Configuration::hideShells() const {
        return d->hideShells;
    }

    const QString &Configuration::lastUser() const {
        return d->lastUser;
    }

    void Configuration::setLastUser(const QString &lastUser) {
        if (d->rememberLastUser)
            d->lastUser = lastUser;
    }

    const QString &Configuration::autoUser() const {
        return d->autoUser;
    }

    bool Configuration::autoRelogin() const {
        return d->autoRelogin;
    }
}
