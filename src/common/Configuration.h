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
        Entry(DefaultPath,         QString,     _S("/bin:/usr/bin:/usr/local/bin"),         _S("Default path to set after successfully logging in"));
        Entry(CursorTheme,         QString,     QString(),                                  _S("Name of the cursor theme to be set before starting the display server"));
        Entry(ServerPath,          QString,     _S("/usr/bin/X"),                           _S("Path of the X server"));
        Entry(XauthPath,           QString,     _S("/usr/bin/xauth"),                       _S("Path of the Xauth"));
        Entry(HaltCommand,         QString,     _S(HALT_COMMAND),                           _S("Halt command"));
        Entry(RebootCommand,       QString,     _S(REBOOT_COMMAND),                         _S("Reboot command"));
        Entry(SessionsDir,         QString,     _S("/usr/share/xsessions"),                 _S("Path of the directory containing session files, e.g kde-plasma.desktop"));
        Entry(RememberLastSession, bool,        true,                                       _S("If this flag is true, LastSession value will updated on every successful login\n"
                                                                                               "Default value is true"));
        Entry(SessionCommand,      QString,     _S(DATA_INSTALL_DIR "/scripts/Xsession"),   _S("Path of script to execute when starting the desktop session"));
        Entry(DisplayCommand,      QString,     _S(DATA_INSTALL_DIR "/scripts/XSetup"),     _S("Path of script to execute when starting the display server"));
        Entry(FacesDir,            QString,     _S(DATA_INSTALL_DIR "/faces"),              _S("Path of the directory containing face files\n"
                                                                                               "Face files should be in username.face.icon format"));
        Entry(ThemesDir,           QString,     _S(DATA_INSTALL_DIR "/themes"),             _S("Path of the directory containing theme files"));
        Entry(CurrentTheme,        QString,     _S("maui"),                                 _S("Name of the current theme"));
        Entry(MinimumUid,          int,         1000,                                       _S("Minimum user id of the users to be listed in the user interface"));
        Entry(MaximumUid,          int,         65000,                                      _S("Maximum user id of the users to be listed in the user interface"));
        Entry(HideUsers,           QStringList, QStringList(),                              _S("Users that shouldn't show up in the user list"));
        Entry(HideShells,          QStringList, QStringList(),                              _S("Shells of users that shouldn't show up in the user list"));
        Entry(RememberLastUser,    bool,        true,                                       _S("If this flag is true, LastUser value will updated on every successful login\n"
                                                                                               "Default value is true"));
        Entry(AutoUser,            QString,     QString(),                                  _S("Name of the user to automatically log in when the system starts for the first time"));
        Entry(AutoSession,         QString,     QString(),                                  _S("Name of the session file to log into automatically."));
        Entry(AutoRelogin,         bool,        false,                                      _S("If true and AutoUser is set, automatic login will kick in again, otherwise autologin will work only for the first time"));
        Entry(MinimumVT,           int,         MINIMUM_VT,                                 _S("Minimum virtual terminal number that will be used by the first display.\n"
                                                                                               "Virtual terminal number will increase as new displays are added."));
        Entry(Numlock,             NumState,    NUM_NONE,                                   _S("Change numlock state when sddm-greeter starts\n"
                                                                                               "Valid values: on|off|none\n"
                                                                                               "If property is set to none, numlock won't be changed"));
//         // SECTION EXAMPLE
//         //      Name         Entries (but anything else too, it's a class)
//         Section(TestSection,
//             Entry(TestEntry1,      QString,     _S("TestStringInSource"),                   _S("TestDescription"));
//         );
    );

    Config(StateConfig, []()->QString{auto tmp = getpwnam("sddm"); return tmp ? tmp->pw_dir : STATE_DIR;}().append("/state.conf"),
        Entry(LastSession,         QString,     QString(),                                  _S("Name of the session file of the last session selected. This session will be preselected when the login screen shows up."));
        Entry(LastUser,            QString,     QString(),                                  _S("Name of the last logged-in user. This username will be preselected/shown when the login screen shows up"));
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
