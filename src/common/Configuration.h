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
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtCore/QDir>
#include <pwd.h>

#include "Constants.h"

#include "ConfigReader.h"

namespace SDDM {
    //     Name        File         Sections and/or Entries (but anything else too, it's a class) - Entries in a Config are assumed to be in the General section
    Config(MainConfig, QStringLiteral(CONFIG_FILE), QStringLiteral(CONFIG_DIR), QStringLiteral(SYSTEM_CONFIG_DIR),
        enum NumState { NUM_NONE, NUM_SET_ON, NUM_SET_OFF };

        //  Name                   Type         Default value                                   Description
        // TODO: Change default to x11-user in a future release
        Entry(DisplayServer,       QString,     _S("x11"),                                      _S("Which display server should be used.\n"
                                                                                                   "Valid values are: x11, x11-user, wayland."));
        Entry(HaltCommand,         QString,     _S(HALT_COMMAND),                               _S("Halt command"));
        Entry(RebootCommand,       QString,     _S(REBOOT_COMMAND),                             _S("Reboot command"));
        Entry(Numlock,             NumState,    NUM_NONE,                                       _S("Initial NumLock state. Can be on, off or none.\n"
                                                                                                   "If property is set to none, numlock won't be changed\n"
                                                                                                   "NOTE: Currently ignored if autologin is enabled."));
        Entry(InputMethod,         QString,     QStringLiteral("qtvirtualkeyboard"),                   _S("Input method module"));
        Entry(Namespaces,          QStringList, QStringList(),                                  _S("Comma-separated list of Linux namespaces for user session to enter"));
        Entry(GreeterEnvironment,  QStringList, QStringList(),                                  _S("Comma-separated list of environment variables to be set"));
        //  Name   Entries (but it's a regular class again)
        Section(Theme,
            Entry(ThemeDir,            QString,     _S(DATA_INSTALL_DIR "/themes"),             _S("Theme directory path"));
            Entry(Current,             QString,     _S(""),                                     _S("Current theme name"));
            Entry(FacesDir,            QString,     _S(DATA_INSTALL_DIR "/faces"),              _S("Global directory for user avatars\n"
                                                                                                   "The files should be named <username>.face.icon"));
            Entry(CursorTheme,         QString,     QString(),                                  _S("Cursor theme used in the greeter"));
            Entry(Font,                QString,     QString(),                                  _S("Font used in the greeter"));
            Entry(EnableAvatars,       bool,        true,                                       _S("Enable display of custom user avatars"));
            Entry(DisableAvatarsThreshold,int,      7,                                          _S("Number of users to use as threshold\n"
                                                                                                   "above which avatars are disabled\n"
                                                                                                   "unless explicitly enabled with EnableAvatars"));
        );

        // TODO: Not absolutely sure if everything belongs here. Xsessions, VT and probably some more seem universal
        Section(X11,
            Entry(ServerPath,          QString,     _S("/usr/bin/X"),                           _S("Path to X server binary"));
            Entry(ServerArguments,     QString,     _S("-nolisten tcp"),                        _S("Arguments passed to the X server invocation"));
            Entry(XephyrPath,          QString,     _S("/usr/bin/Xephyr"),                      _S("Path to Xephyr binary"));
            Entry(XauthPath,           QString,     _S("/usr/bin/xauth"),                       _S("Path to xauth binary"));
            Entry(SessionDir,          QString,     _S("/usr/share/xsessions"),                 _S("Directory containing available X sessions"));
            Entry(SessionCommand,      QString,     _S(SESSION_COMMAND),                        _S("Path to a script to execute when starting the desktop session"));
	    Entry(SessionLogFile,      QString,     _S(".local/share/sddm/xorg-session.log"),   _S("Path to the user session log file"));
	    Entry(UserAuthFile,        QString,     _S(".Xauthority"),                          _S("Path to the Xauthority file"));
            Entry(DisplayCommand,      QString,     _S(DATA_INSTALL_DIR "/scripts/Xsetup"),     _S("Path to a script to execute when starting the display server"));
            Entry(DisplayStopCommand,  QString,     _S(DATA_INSTALL_DIR "/scripts/Xstop"),      _S("Path to a script to execute when stopping the display server"));
            Entry(EnableHiDPI,         bool,        false,                                      _S("Enable Qt's automatic high-DPI scaling"));
        );

        Section(Wayland,
            Entry(CompositorCommand,   QString,     _S("weston --shell=fullscreen-shell.so"),   _S("Path of the Wayland compositor to execute when starting the greeter"));
            Entry(SessionDir,          QString,     _S("/usr/share/wayland-sessions"),          _S("Directory containing available Wayland sessions"));
            Entry(SessionCommand,      QString,     _S(WAYLAND_SESSION_COMMAND),                _S("Path to a script to execute when starting the desktop session"));
	    Entry(SessionLogFile,      QString,     _S(".local/share/sddm/wayland-session.log"),_S("Path to the user session log file"));
            Entry(EnableHiDPI,         bool,        false,                                      _S("Enable Qt's automatic high-DPI scaling"));
        );

        Section(Users,
            Entry(DefaultPath,         QString,     _S("/usr/local/bin:/usr/bin:/bin"),         _S("Default $PATH for logged in users"));
            Entry(MinimumUid,          int,         UID_MIN,                                    _S("Minimum user id for displayed users"));
            Entry(MaximumUid,          int,         UID_MAX,                                    _S("Maximum user id for displayed users"));
            Entry(HideUsers,           QStringList, QStringList(),                              _S("Comma-separated list of users that should not be listed"));
            Entry(HideShells,          QStringList, QStringList(),                              _S("Comma-separated list of shells.\n"
                                                                                                   "Users with these shells as their default won't be listed"));
            Entry(RememberLastUser,    bool,        true,                                       _S("Remember the last successfully logged in user"));
            Entry(RememberLastSession, bool,        true,                                       _S("Remember the session of the last successfully logged in user"));

            Entry(ReuseSession,        bool,        true,                                       _S("When logging in as the same user twice, restore the original session, rather than create a new one"));
        );

        Section(Autologin,
            Entry(User,                QString,     QString(),                                  _S("Username for autologin session"));
            Entry(Session,             QString,     QString(),                                  _S("Name of session file for autologin session (if empty try last logged in)"));
            Entry(Relogin,             bool,        false,                                      _S("Whether sddm should automatically log back into sessions when they exit"));
        );

        Section(Fingerprintlogin,
            Entry(User,                QString,     QString(),                                  _S("Username for default user"));
            Entry(Session,             QString,     QString(),                                  _S("Name of session file for fingerprint login session (if empty try last logged in)"));
        );
    );

    Config(StateConfig, []()->QString{auto tmp = getpwnam("sddm"); return tmp ? QString::fromLocal8Bit(tmp->pw_dir) : QStringLiteral(STATE_DIR);}().append(QStringLiteral("/state.conf")), QString(), QString(),
        Section(Last,
            Entry(Session,         QString,     QString(),                                      _S("Name of the session for the last logged-in user.\n"
                                                                                                   "This session will be preselected when the login screen appears."));
            Entry(User,            QString,     QString(),                                      _S("Name of the last logged-in user.\n"
                                                                                                   "This user will be preselected when the login screen appears"));
        );
    );

    extern MainConfig mainConfig;
    extern StateConfig stateConfig;

    inline QTextStream& operator>>(QTextStream &str, MainConfig::NumState &state) {
        QString text = str.readLine().trimmed();
        if (text.compare(QLatin1String("on"), Qt::CaseInsensitive) == 0)
            state = MainConfig::NUM_SET_ON;
        else if (text.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
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
