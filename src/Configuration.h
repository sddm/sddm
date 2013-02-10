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

#ifndef SDE_CONFIGURATION_H
#define SDE_CONFIGURATION_H

#include <QString>
#include <QStringList>

namespace SDE {
    class ConfigurationPrivate;

    class Configuration {
    public:
        Configuration(const QString &configPath);
        ~Configuration();

        void load();
        void save();

        static Configuration *instance();

        const QString &defaultPath() const;

        const QString &serverPath() const;
        const QStringList &serverArgs() const;

        const QString &xauthPath() const;

        const QString &authFile() const;
        const QString &lockFile() const;

        const QString &haltCommand() const;
        const QString &rebootCommand() const;

        const QString &sessionsDir() const;

        const QString &lastSession() const;
        void setLastSession(const QString &lastSession);

        const QString &sessionCommand() const;

        const QString &themesDir() const;
        const QString &currentTheme() const;

        const int minimumUid() const;

        const QString &lastUser() const;
        void setLastUser(const QString &lastUser);

        const QString &autoUser() const;

    private:
        ConfigurationPrivate *d { nullptr };
    };
}
#endif // SDE_CONFIGURATION_H
