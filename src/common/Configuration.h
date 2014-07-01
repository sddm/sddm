/*
 * SDDM configuration
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
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

#ifndef SDDM_CONFIGURATION_H
#define SDDM_CONFIGURATION_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtCore/QDir>
#include <pwd.h>

#include "Constants.h"

#include "ConfigReader.h"

namespace SDDM {
    //     Name        File         Sections and/or Entries (but anything else too, it's a class) - Entries in a Config are assumed to be in the General section
    Config(MainConfig, CONFIG_FILE,
        enum NumState { NUM_NONE, NUM_SET_ON, NUM_SET_OFF };

        //    Name                 Type         Default value                               Description
        Entry(HaltCommand,         QString,     _S(HALT_COMMAND),                           _S("Halt command"));
        Entry(RebootCommand,       QString,     _S(REBOOT_COMMAND),                         _S("Reboot command"));
        Entry(Numlock,             NumState,    NUM_NONE,                                   _S("Initial NumLock state\n"
                                                                                               "Valid values: on|off|none\n"
                                                                                               "If property is set to none, numlock won't be changed"));
        //      Name   Entries (but it's a regular class again)
        Section(Theme,
            Entry(ThemeDir,            QString,     _S(DATA_INSTALL_DIR "/themes"),             _S("Theme directory path"));
            Entry(Current,             QString,     _S("maui"),                                 _S("Current theme name"));
            Entry(FacesDir,            QString,     _S(DATA_INSTALL_DIR "/faces"),              _S("Face icon directory\n"
                                                                                                   "The files should be in username.face.icon format"));
            Entry(CursorTheme,         QString,     QString(),                                  _S("Cursor theme"));
        );
        // TODO: Not absolutely sure if everything belongs here. Xsessions, VT and probably some more seem universal
        Section(XDisplay,
            Entry(ServerPath,          QString,     _S("/usr/bin/X"),                           _S("X server path"));
            Entry(XauthPath,           QString,     _S("/usr/bin/xauth"),                       _S("Xauth path"));
            Entry(SessionDir,          QString,     _S("/usr/share/xsessions"),                 _S("Session description directory"));
            Entry(SessionCommand,      QString,     _S(DATA_INSTALL_DIR "/scripts/Xsession"),   _S("XSession script path\n"
                                                                                                   "A script to execute when starting the desktop session"));
            Entry(DisplayCommand,      QString,     _S(DATA_INSTALL_DIR "/scripts/XSetup"),     _S("XSetup script path\n"
                                                                                                   "A script to execute when starting the display server"));
            Entry(MinimumVT,           int,         MINIMUM_VT,                                 _S("Minimum VT\n"
                                                                                                   "The lowest virtual terminal number that will be used."));
        );
        Section(Users,
            Entry(DefaultPath,         QString,     _S("/bin:/usr/bin:/usr/local/bin"),         _S("Default $PATH"));
            Entry(MinimumUid,          int,         1000,                                       _S("Minimum user id for displayed users"));
            Entry(MaximumUid,          int,         65000,                                      _S("Maximum user id for displayed users"));
            Entry(HideUsers,           QStringList, QStringList(),                              _S("Hidden users"));
            Entry(HideShells,          QStringList, QStringList(),                              _S("Hidden shells\n"
                                                                                                   "Users with these shells as their default won't be listed"));
            Entry(RememberLastUser,    bool,        true,                                       _S("Remember the last successfully logged in user"));
            Entry(RememberLastSession, bool,        true,                                       _S("Remember the session of the last successfully logged in user"));
        );
        Section(Autologin,
            Entry(User,                QString,     QString(),                                  _S("Autologin user"));
            Entry(Session,             QString,     QString(),                                  _S("Autologin session"));
            Entry(Relogin,             bool,        false,                                      _S("Autologin again on session exit"));
        );
    );

    Config(StateConfig, []()->QString{auto tmp = getpwnam("sddm"); return tmp ? tmp->pw_dir : STATE_DIR;}().append("/state.conf"),
        Section(Last,
            Entry(Session,         QString,     QString(),                                  _S("Name of the session file of the last session selected. This session will be preselected when the login screen shows up."));
            Entry(User,            QString,     QString(),                                  _S("Name of the last logged-in user. This username will be preselected/shown when the login screen shows up"));
        );
    );

    extern MainConfig mainConfig;
    extern StateConfig stateConfig;

    inline QTextStream& operator>>(QTextStream &str, MainConfig::NumState &state) {
        QString text = str.readLine().trimmed();
        if (text.compare("on", Qt::CaseInsensitive))
            state = MainConfig::NUM_SET_ON;
        else if (text.compare("off", Qt::CaseInsensitive))
            state = MainConfig::NUM_SET_OFF;
        else
            state = MainConfig::NUM_NONE;
        return str;
    }

    inline QTextStream& operator<<(QTextStream &str, const MainConfig::NumState &state) {
        if (state == MainConfig::NUM_SET_ON)
            str << "on";
        else if (state == MainConfig::NUM_SET_OFF)
            str << "off";
        else
            str << "none";
        return str;
    }
}

#endif // SDDM_CONFIGURATION_H
