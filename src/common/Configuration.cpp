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
#include "Constants.h"

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

        QString stateDir { "" };

        QString haltCommand { "" };
        QString rebootCommand { "" };

        QString sessionsDir { "" };
        bool rememberLastSession { true };
        QString lastSession { "" };
        QString sessionCommand { "" };
        QString displayCommand { "" };

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

        Configuration::NumState numlock { Configuration::NUM_NONE };
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

    QString appendSlash(const QString path) {
        if (path.isEmpty() || path.endsWith('/')) {
            return path;
        } else {
            return path + '/';
        }
    }

    void Configuration::load() {
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // read settings
        d->cursorTheme = settings.value("CursorTheme", "").toString();
        d->defaultPath = settings.value("DefaultPath", "").toString();
        d->serverPath = settings.value("ServerPath", "").toString();
        d->xauthPath = settings.value("XauthPath", "").toString();
        d->stateDir = settings.value("StateDir", STATE_DIR).toString();
        d->haltCommand = settings.value("HaltCommand", "").toString();
        d->rebootCommand = settings.value("RebootCommand", "").toString();
        d->sessionsDir = appendSlash(settings.value("SessionsDir", "").toString());
        d->rememberLastSession = settings.value("RememberLastSession", d->rememberLastSession).toBool();
        d->lastSession = settings.value("LastSession", "").toString();
        d->sessionCommand = settings.value("SessionCommand", "").toString();
        d->displayCommand = settings.value("DisplayCommand", "").toString();
        d->facesDir = appendSlash(settings.value("FacesDir", "").toString());
        d->themesDir = appendSlash(settings.value("ThemesDir", "").toString());
        d->currentTheme = settings.value("CurrentTheme", "").toString();
        d->minimumUid = settings.value("MinimumUid", "0").toInt();
        d->maximumUid = settings.value("MaximumUid", "65000").toInt();
        d->hideUsers = settings.value("HideUsers", "").toString().split(' ', QString::SkipEmptyParts);
        d->hideShells = settings.value("HideShells", "").toString().split(' ', QString::SkipEmptyParts);
        d->rememberLastUser = settings.value("RememberLastUser", d->rememberLastUser).toBool();
        d->lastUser = settings.value("LastUser", "").toString();
        d->autoUser = settings.value("AutoUser", "").toString();
        d->autoRelogin = settings.value("AutoRelogin", d->autoRelogin).toBool();
        minimumVT = settings.value("MinimumVT", minimumVT).toUInt();

        QString num_val = settings.value("Numlock", "none").toString().toLower();
        if (num_val == "on") {
            d->numlock = Configuration::NUM_SET_ON;
        } else if (num_val == "off") {
            d->numlock = Configuration::NUM_SET_OFF;
        } else {
            d->numlock = Configuration::NUM_NONE;
        }
    }

    void Configuration::save() {
        // create settings object
        QSettings settings(d->configPath, QSettings::IniFormat);
        // write settings back
        settings.setValue("CursorTheme", d->cursorTheme);
        settings.setValue("DefaultPath", d->defaultPath);
        settings.setValue("ServerPath", d->serverPath);
        settings.setValue("XauthPath", d->xauthPath);
        settings.setValue("StateDir", d->stateDir);
        settings.setValue("HaltCommand", d->haltCommand);
        settings.setValue("RebootCommand", d->rebootCommand);
        settings.setValue("SessionsDir", d->sessionsDir);
        settings.setValue("RememberLastSession", d->rememberLastSession);
        settings.setValue("LastSession", d->lastSession);
        settings.setValue("SessionCommand", d->sessionCommand);
        settings.setValue("DisplayCommand", d->displayCommand);
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
        settings.setValue("MinimumVT", minimumVT);

        if (d->numlock == NUM_NONE)
            settings.setValue("Numlock", "none");
        else if (d->numlock == NUM_SET_ON)
            settings.setValue("Numlock", "on");
        else if (d->numlock == NUM_SET_OFF)
            settings.setValue("Numlock", "off");
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

    QString Configuration::stateDir() const {
        // use "." as stateDir in test mode to
        // avoid permission denied errors
        if (testing)
            return QLatin1String(".");
        return d->stateDir;
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

    const QString &Configuration::displayCommand() const {
        return d->displayCommand;
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

    QString Configuration::currentThemePath() const {
        return d->themesDir + d->currentTheme;
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

    const Configuration::NumState Configuration::numlock() const {
        return d->numlock;
    }
}
