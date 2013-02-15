__INTRODUCTION__

SDDM is a new lightweight display manager for X11 aiming to be fast, simple and beatiful.

One of the distinctive features of SDDM is the ability to use QML for user interface creation. QML is a JavaScript-based, declarative language for designing user interface–centric applications. It is designed to provide highly customizable user interfaces with fluid animations. It supports images, gradients, color/size/opacity/property animations, hardware acceleration and lots of other stuff needed to create beatiful interfaces by today's standards.

SDDM has a small, simple and hackable codebase.

__DEPENDENCIES__

SDDM depends on PAM for authorization. SDDM uses a PAM service named "sddm" to authorize user. PAM services are created using simple config files installed into PAM config directory (e.g /etc/pam.d). A sample service file named "sddm.pam" is provided. You should copy this file into your PAM config directory and rename it to "sddm". Since the authorization procedure and available PAM modules may change depending on your distribution or your setup, we strongly advise reviewing this service file to see if it fits your needs.

SDDM depends on Xlib for communicating to the X server. Also we depend on Qt for loading the user interface and event loop management, apart from other things.

__COMPILATION__

SDDM uses CMake for compile configuration. Typical compilation procedure for a cmake-based project is:

`mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && make`

SDDM can be compiled with Qt4 or Qt5. Default is Qt4. But setting USE_QT5 flag you can compile with Qt5.

`mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DUSE_QT5=true && make`

__CONFIGURATION__

SDDM configuration is done using a simple ini-style text file. Config file usually resides in /etc/sddm.conf. Location of the config file can be changed when compiling the project. You can use a different configuration file at runtime using the "-c" parameter.

`sddm -c /etc/sddm-alternative.conf`

Configuration file is self documented. See the comments in the file for available options.

__THEMES__

SDDM themes are a collection of qml files and needed resources residing in a directory. There is almost no restrictions internal structure or layout of the files. But every theme must have a _metadata.desktop_ file in the theme directory. This file contains the metadata of the theme, e.g authors, entry point etc.

__SESSION MANAGER__

This class contains several functions useful for the themes and can be accessed through a context property named _sessionManager_ within the themes. 

__hostName__: Host name of the computer SDDM running on. You can use this property, for example for a welcome message.

`Text { text: qsTr("Welcome to ") + sessionManager.hostName }`

__lastUser__: This property holds the last user successfully logged into the system through SDDM. You can use this, for example to preselect a user from a list or put into the user name field as default.

` TextBox {  id: name; text: sessionManager.lastUser }`

__login(string username, string password, int sessionIndex)__: This functions tries to authenticate using given username and password. If authentication fails, __fail__ signal is emitted. If authentication is successfull session manager emits __success__ signal, closes the user interface and executes session command for the session with the index __sessionIndex__.

__shuthdown()__: Shutdowns the computer.

__reboot()__: Reboots the computer

__fail__: This signal is emitted when authentication fails. Can be used to show an error message.

__success__: This signal is emitted when authentication succeeds.

__SESSION MODEL__

This class is a list model containing available x-sessions on the system. Session list is created using the desktop files installed into the xsessions directory (e.g /usr/share/xsessions). The desktop files are generally installed along with a desktop environment like KDE or XFCE.

Session model with QML views directly. This class also contains index of the last session used through property _lastIndex_.

__USER MODEL__

This class is a list model containing available users on the system. User list is created by parsing the /etc/passwd file. Like session model this model also can be used with QML views directly.

__TESTING__

To test your themes use the "-t" commandline parameter.

`sddm -t /path/to/your/theme`

__LICENSING__

Source code of SDDM is licensed under GNU GPL version 2 or later (at your opinion). Scripts and configuration files are public domain. QML files are MIT licensed and images are CC BY 3.0.

__RESOURCES__

Git Repository:
    https://github.com/sddm/sddm

Mailing List:
    https://groups.google.com/group/sde-devel

Bug Reports:
    https://github.com/sddm/sddm/issues

Wiki:
    https://github.com/sddm/sddm/wiki

__Screenshots__

![sample screenshot](https://raw.github.com/sddm/sddm/master/data/themes/maui/screenshot.jpg)
