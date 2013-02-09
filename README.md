__Introduction__

SDDM is a new lightweight display manager for X11 aiming to be fast, simple and beatiful.

One of the distinctive features of SDDM is the ability to use QML for user interface creation. QML is a JavaScript-based, declarative language for designing user interface–centric applications. It is designed to provide highly customizable user interfaces with fluid animations. It supports images, gradients, color/size/opacity/property animations, hardware acceleration and lots of other stuff needed to create beatiful interfaces by today's standards.

SDDM's code base is tiny: under 1000 lines of code.

__Dependencies__

SDDM depends on PAM for authorization. SDDM uses a PAM service named "sddm" to authorize user. PAM services are created using simple config files installed into PAM config directory (e.g /etc/pam.d). A sample service file named "sddm.pam" is provided. You should copy this file into your PAM config directory and rename it to "sddm". Since the authorization procedure and available PAM modules may change depending on your distribution or your setup, we strongly advise reviewing this service file to see if it fits your needs.

SDDM depends on Xlib for communicating to the X server. Also we depend on Qt for loading the user interface and event loop management, apart from other things.

__Compilation__

SDDM uses CMake for compile configuration. Typical compilation procedure for a cmake-based project is:

`mkdir build && cd build && cmake .. && make`

SDDM by default compiles for Qt4 but supports compiling for Qt5 too. If you want to use Qt5, USE_QT5 arguments must be given to cmake:

`mkdir build && cd build && cmake .. -DUSE_QT5=true && make`

Qt4 and Qt5 based themes are incompatible. SDDM chooses correct theme on installation.

__Configuration__

SDDM configuration is done using a simple ini-style text file. Config file usually resides in /etc/sddm.conf. Location of the config file can be changed when compiling the project. You can use a different configuration file at runtime using the "-c" parameter.

`sddm -c /etc/sddm-alternative.conf`

Configuration file is self documented. See the comments in the file for available options.

__Themes__

SDDM themes are a collection of qml files and needed resources residing in a directory. There is almost no restrictions internal structure or layout of the files. But the main qml file for the theme must have the name "Main.qml".

There are several functions provided by SDDM to the theme to enable authorization/shutdown/reboot. These functions are available to the theme through a context property named _sessionManager_. Session manager provides following:

__Properties__

__hostName__: Host name of the computer SDDM running on. You can use this property, for example for a welcome message.

`Text { text: qsTr("Welcome to ") + sessionManager.hostName }`

__lastUser__: This property holds the last user successfully logged into the system through SDDM. You can use this, for example to preselect a user from a list or put into the user name field as default.

` TextBox {  id: name; text: sessionManager.lastUser }`

__sessions__: This property is list of strings corresponding to the names of the desktop xsessions available in the system. This session list comes from the desktop files installed into the xsessions directory (e.g /usr/share/xsessions). The desktop files are generally installed along with a desktop environment like KDE or XFCE. You can use this property to show a user a list of available sessions to select from.

__lastSession__: Similar to the __lastUser__, this property holds _position_ of the last session used, in the __sessions__ list. This property can have a value of _0_ to _sessions.size - 1_.

__Functions__

__login(string username, string password, int sessionIndex)__: This functions tries to authenticate using given username and password. If authentication fails, __fail__ signal is emitted. If authentication is successfull session manager emits __success__ signal, closes the user interface and executes session command for the session with the index __sessionIndex__.

__shuthdown()__: Shutdowns the computer.

__reboot()__: Reboots the computer

__Signals__

__fail__: This signal is emitted when authentication fails. Can be used to show an error message.

__success__: This signal is emitted when authentication succeeds.

__Testing__

To test your themes use the "-t" commandline parameter.

`sddm -t /path/to/your/theme`

__Licensing__

Source code of SDDM is licensed under GNU GPL version 2 or later (at your opinion). Scripts and configuration files are public domain. QML files are MIT licensed and images are CC BY 3.0.

__Resources__

Git Repository:
    https://github.com/sddm/sddm

Mailing List:
    https://groups.google.com/group/sde-devel

Bug Reports:
    https://github.com/sddm/sddm/issues

Wiki:
    https://github.com/sddm/sddm/wiki

__Screenshots__

![sample screenshot](https://raw.github.com/sddm/sddm/master/themes/Maldives/screenshot.jpg)
