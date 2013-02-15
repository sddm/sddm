# Default path to set after successfully logging in
DefaultPath=/bin:/usr/bin:/usr/local/bin

# Name of the cursor theme to be set before starting
# the display server
CursorTheme=""

# Path of the X server
ServerPath=/usr/bin/X

# Arguments to be given to the X server. Note that
# -auth parameter is appended automatically
ServerArgs=-nolisten tcp vt07

# Path of the Xauth
XauthPath=/usr/bin/xauth

# Path of the auth file to be passed to the X server
AuthFile=/var/run/sddm.auth

# Path of the lock file
LockFile=/var/run/sddm.pid

# Halt and reboot commands
HaltCommand=/sbin/shutdown -h -P now
RebootCommand=/sbin/shutdown -r now

# Path of the directory containing session files,
# e.g kde-plasma.desktop
SessionsDir=/usr/share/xsessions

# Name of the session file of the last session
# selected. This session will be preselected when
# the login screen shows up.
LastSession=

# Path of script to execute when starting the desktop session
SessionCommand=${DATA_INSTALL_DIR}/scripts/Xsession

# Path of the directory containing face files
# Face files should be in username.face.icon format
FacesDir=${DATA_INSTALL_DIR}/faces

# Path of the directory containing theme files
ThemesDir=${DATA_INSTALL_DIR}/themes

# Name of the current theme
CurrentTheme=maui

# Minimum user id of the users to be listed in the
# user interface
MinimumUid=1000

# Name of last logged-in user. This username will be
# preselected/shown when the login screen shows up.
LastUser=


# Name of the user to automatically log in when the
# system starts first time.
AutoUser=
