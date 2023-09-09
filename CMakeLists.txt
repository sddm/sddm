cmake_minimum_required(VERSION 3.4)

project(SDDM)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Set version
set(SDDM_VERSION_MAJOR 0)
set(SDDM_VERSION_MINOR 20)
set(SDDM_VERSION_PATCH 0)
set(SDDM_VERSION_STRING "${SDDM_VERSION_MAJOR}.${SDDM_VERSION_MINOR}.${SDDM_VERSION_PATCH}")
add_compile_definitions("SDDM_VERSION=\"${SDDM_VERSION_STRING}\"")

# Options
option(BUILD_MAN_PAGES "Build man pages" OFF)
option(ENABLE_JOURNALD "Enable logging to journald" ON)
option(NO_SYSTEMD "Disable systemd support" OFF)
option(USE_ELOGIND "Use elogind instead of logind" OFF)
option(BUILD_WITH_QT6 "Build with Qt 6" OFF)
option(INSTALL_PAM_CONFIGURATION "Install PAM configuration files" ON)
option(BUILD_GREETER_QT5 "Build Qt 5 greeter if Qt 6 is used" ON)
option(BUILD_GREETER_QT6 "Build Qt 6 greeter if Qt 5 is used" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# Definitions
add_definitions(-Wall -Wextra -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_FOREACH)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Handle build type
if(CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
    message(STATUS "Debug build")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    message(STATUS "Release build")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# Default absolute paths
if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()
if(NOT DEFINED CMAKE_INSTALL_LOCALSTATEDIR)
    set(CMAKE_INSTALL_LOCALSTATEDIR "/var")
endif()

# Standard installation paths
include(GNUInstallDirs)

# Macros
include(FeatureSummary)

# PKG-CONFIG
find_package(PkgConfig)

# PAM
find_package(PAM REQUIRED)

# XAU
pkg_check_modules(LIBXAU REQUIRED "xau")

# XCB
find_package(XCB REQUIRED)

# XKB
find_package(XKB REQUIRED)

# systemd
if(NOT NO_SYSTEMD AND NOT USE_ELOGIND)
    pkg_check_modules(SYSTEMD "systemd")
endif()

if(SYSTEMD_FOUND)
    add_definitions(-DHAVE_SYSTEMD)
    set(CMAKE_AUTOMOC_MOC_OPTIONS -DHAVE_SYSTEMD)

    # libsystemd-journal was merged into libsystemd in 209
    if(${SYSTEMD_VERSION} VERSION_LESS 209)
        pkg_check_modules(JOURNALD "libsystemd-journal")
    else()
        pkg_check_modules(JOURNALD "libsystemd")
    endif()

    if(ENABLE_JOURNALD)
        if(JOURNALD_FOUND)
            add_definitions(-DHAVE_JOURNALD)
            set(CMAKE_AUTOMOC_MOC_OPTIONS -DHAVE_JOURNALD)
        else()
            message(WARNING "Disable journald support for lack of libsystemd-journal")
        endif()
    endif()

    if (NOT DEFINED SYSTEMD_SYSTEM_UNIT_DIR)
        pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
    endif()

    if (NOT DEFINED SYSTEMD_SYSUSERS_DIR)
        pkg_get_variable(SYSTEMD_SYSUSERS_DIR systemd sysusersdir)
    endif()

    if (NOT DEFINED SYSTEMD_TMPFILES_DIR)
        pkg_get_variable(SYSTEMD_TMPFILES_DIR systemd tmpfilesdir)
    endif()

    set(HALT_COMMAND "/usr/bin/systemctl poweroff")
    set(REBOOT_COMMAND "/usr/bin/systemctl reboot")
else()
    set(SYSTEMD_FOUND 0)
endif()
add_feature_info("systemd" SYSTEMD_FOUND "systemd support")
add_feature_info("journald" JOURNALD_FOUND "journald support")

# elogind
if(NO_SYSTEMD AND USE_ELOGIND)
    pkg_check_modules(ELOGIND "libelogind")
endif()

if(ELOGIND_FOUND)
    add_definitions(-DHAVE_ELOGIND)
    set(CMAKE_AUTOMOC_MOC_OPTIONS -DHAVE_ELOGIND)

    set(HALT_COMMAND "/usr/bin/loginctl poweroff")
    set(REBOOT_COMMAND "/usr/bin/loginctl reboot")
endif()
add_feature_info("elogind" ELOGIND_FOUND "elogind support")

# Default behaviour if neither systemd nor elogind is used
if (NOT ELOGIND_FOUND AND NOT SYSTEMD_FOUND)
    # Set the VT on which sddm will normally appear, and the
    # commands for shutdown and reboot. On FreeBSD, there are
    # normally more getty's running than on Linux.
    if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
        set(HALT_COMMAND "/sbin/shutdown -p now")
    else()
        set(HALT_COMMAND "/sbin/shutdown -h -P now")
    endif()
    set(REBOOT_COMMAND "/sbin/shutdown -r now")
endif()

if (SYSTEMD_FOUND)
    set(RUNTIME_DIR_DEFAULT "/run/sddm")
else()
    set(RUNTIME_DIR_DEFAULT "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/run/sddm")
endif()

# Set constants
set(DATA_INSTALL_DIR            "${CMAKE_INSTALL_FULL_DATADIR}/sddm"                CACHE PATH      "System application data install directory")
set(DBUS_CONFIG_DIR             "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/system.d"       CACHE PATH      "DBus config files directory")
set(STATE_DIR                   "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/sddm"      CACHE PATH      "State directory")
set(RUNTIME_DIR                 "${RUNTIME_DIR_DEFAULT}"                            CACHE PATH      "Runtime data storage directory")

set(SESSION_COMMAND             "${DATA_INSTALL_DIR}/scripts/Xsession"              CACHE PATH      "Script to execute when starting the X11 desktop session")
set(WAYLAND_SESSION_COMMAND     "${DATA_INSTALL_DIR}/scripts/wayland-session"       CACHE PATH      "Script to execute when starting the Wayland desktop session")

set(CONFIG_FILE                 "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf"        CACHE PATH      "Path of the sddm config file")
set(CONFIG_DIR                  "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf.d"      CACHE PATH      "Path of the sddm config directory")
set(ACCOUNTSSERVICE_DATA_DIR    "/var/lib/AccountsService"                          CACHE PATH      "Path of the accountsservice data directory")
set(SYSTEM_CONFIG_DIR           "${CMAKE_INSTALL_PREFIX}/lib/sddm/sddm.conf.d"      CACHE PATH      "Path of the system sddm config directory")
set(LOG_FILE                    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/sddm.log"  CACHE PATH      "Path of the sddm log file")
set(DBUS_CONFIG_FILENAME        "org.freedesktop.DisplayManager.conf"               CACHE STRING    "Name of the sddm config file")
set(SDDM_INITIAL_VT             "1"                                                 CACHE STRING    "Initial tty to use")


# Autodetect UID_MIN and UID_MAX from /etc/login.defs
if(NOT DEFINED LOGIN_DEFS_PATH)
    set(LOGIN_DEFS_PATH "/etc/login.defs" CACHE PATH "Path to login.defs")
endif()

if(NOT EXISTS "${LOGIN_DEFS_PATH}" AND NOT DEFINED UID_MIN AND NOT DEFINED UID_MAX)
    message(FATAL_ERROR "Could not find login.defs at ${LOGIN_DEFS_PATH}. Try -DLOGIN_DEFS_PATH.")
endif()

if(NOT DEFINED UID_MIN)
    file(STRINGS "${LOGIN_DEFS_PATH}" UID_MIN LIMIT_COUNT 1 REGEX "^UID_MIN")
    string(REGEX REPLACE "^UID_MIN[\t ]+([0-9]+)" "\\1" UID_MIN "${UID_MIN}")
    if(NOT UID_MIN MATCHES "[0-9]+")
        message(WARNING "Could not detect a valid UID_MIN (got '${UID_MIN}'). You should review your login.defs file.")
        set(UID_MIN 1000)
    else()
        message(STATUS "Autodetected UID_MIN=${UID_MIN} from ${LOGIN_DEFS_PATH}")
    endif()
endif()

if(NOT DEFINED UID_MAX)
    file(STRINGS "${LOGIN_DEFS_PATH}" UID_MAX LIMIT_COUNT 1 REGEX "^UID_MAX")
    string(REGEX REPLACE "^UID_MAX[\t ]+([0-9]+)" "\\1" UID_MAX "${UID_MAX}")
    if(NOT UID_MAX MATCHES "[0-9]+")
        message(WARNING "Could not detect a valid UID_MAX (got '${UID_MAX}'). You should review your login.defs file.")
        set(UID_MAX 65000)
    else()
        message(STATUS "Autodetected UID_MAX=${UID_MAX} from ${LOGIN_DEFS_PATH}")
    endif()
endif()

enable_testing()

# Build the greeter for non-main Qt versions.
# Need to do this before the versionless imports by the find_package(Qt...) below.
if(BUILD_WITH_QT6 AND BUILD_GREETER_QT5)
block(SCOPE_FOR VARIABLES)
    set(QT_MAJOR_VERSION 5)
    set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)
    set(QT_NO_CREATE_VERSIONLESS_FUNCTIONS ON)

    find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED Core DBus Gui Qml Quick LinguistTools Test QuickTest)

    add_subdirectory(components components-qt${QT_MAJOR_VERSION})
    add_subdirectory(data/translations translations-qt${QT_MAJOR_VERSION})
    add_subdirectory(src/greeter greeter-qt${QT_MAJOR_VERSION})
    add_subdirectory(test test-qt${QT_MAJOR_VERSION})
endblock()
endif()

if(NOT BUILD_WITH_QT6 AND BUILD_GREETER_QT6)
block(SCOPE_FOR VARIABLES)
    set(QT_MAJOR_VERSION 6)
    set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)
    set(QT_NO_CREATE_VERSIONLESS_FUNCTIONS ON)

    find_package(Qt${QT_MAJOR_VERSION} CONFIG REQUIRED Core DBus Gui Qml Quick LinguistTools Test QuickTest)

    # https://bugreports.qt.io/browse/QTBUG-114706
    add_library(Qt::Core ALIAS Qt6::Core)
    add_library(Qt::Gui ALIAS Qt6::Gui)
    add_library(Qt::Network ALIAS Qt6::Network)
    add_library(Qt::OpenGL ALIAS Qt6::OpenGL)
    add_library(Qt::Test ALIAS Qt6::Test)

    add_subdirectory(components components-qt${QT_MAJOR_VERSION})
    add_subdirectory(data/translations translations-qt${QT_MAJOR_VERSION})
    add_subdirectory(src/greeter greeter-qt${QT_MAJOR_VERSION})
    add_subdirectory(test test-qt${QT_MAJOR_VERSION})
endblock()
endif()

# Qt
if(BUILD_WITH_QT6)
    set(QT_MAJOR_VERSION 6)
    message(STATUS "Building Qt 6 version")
else()
    set(QT_MAJOR_VERSION 5)
    message(STATUS "Building Qt 5 version")
endif()

find_package(Qt${QT_MAJOR_VERSION} 5.15.0 CONFIG REQUIRED Core DBus Gui Qml Quick LinguistTools Test QuickTest)

# Add subdirectories
add_subdirectory(components)
add_subdirectory(data)
add_subdirectory(data/translations)
add_subdirectory(services)
add_subdirectory(src)

add_subdirectory(test)

# Display feature summary
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
