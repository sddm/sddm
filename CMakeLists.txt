cmake_minimum_required(VERSION 2.8.8)

project(SDDM)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Silence CMake 3.0 warnings
if(POLICY CMP0043)
    cmake_policy(SET CMP0043 OLD)
endif()

# Set version
set(SDDM_VERSION_MAJOR 0)
set(SDDM_VERSION_MINOR 18)
set(SDDM_VERSION_PATCH 1)
set(SDDM_VERSION_STRING "${SDDM_VERSION_MAJOR}.${SDDM_VERSION_MINOR}.${SDDM_VERSION_PATCH}")

# Set up packaging
set(CPACK_PACKAGE_NAME "sddm")
set(CPACK_PACKAGE_VERSION "${SDDM_VERSION_STRING}")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SET_DESTDIR FALSE)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git;/*.user;/.tx/;~$;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

# Options
option(BUILD_MAN_PAGES "Build man pages" OFF)
option(ENABLE_JOURNALD "Enable logging to journald" ON)
option(ENABLE_PAM "Enable PAM support" ON)
option(NO_SYSTEMD "Disable systemd support" OFF)
option(USE_ELOGIND "Use elogind instead of logind" OFF)

# ECM
find_package(ECM 1.4.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH};${ECM_MODULE_PATH}")

# Definitions
add_definitions(-Wall -std=c++11 -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)

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
if(ENABLE_PAM)
    find_package(PAM REQUIRED)

    if(PAM_FOUND)
        add_definitions(-DUSE_PAM)
    endif()
endif()
add_feature_info("PAM" PAM_FOUND "PAM support")

# getspnam and shadow(5) support
include(CheckFunctionExists)
check_function_exists(getspnam HAVE_GETSPNAM)

# XCB
find_package(XCB REQUIRED)

# XKB
find_package(XKB REQUIRED)

# Qt 5
find_package(Qt5 5.8.0 CONFIG REQUIRED Core DBus Gui Qml Quick LinguistTools Test)

# find qt5 imports dir
get_target_property(QMAKE_EXECUTABLE Qt5::qmake LOCATION)
if(NOT QT_IMPORTS_DIR)
    exec_program(${QMAKE_EXECUTABLE} ARGS "-query QT_INSTALL_QML" RETURN_VALUE return_code OUTPUT_VARIABLE QT_IMPORTS_DIR)
endif()

# Uninstall target
if ("${ECM_VERSION}" VERSION_LESS "1.7.0")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
                   IMMEDIATE @ONLY)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

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
        execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
        string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SYSTEM_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR})
    endif()

    set(MINIMUM_VT 1)
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

    set(MINIMUM_VT 7)
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
        set(MINIMUM_VT 9)
        set(HALT_COMMAND "/sbin/shutdown -p now")
    else()
        set(MINIMUM_VT 7)
        set(HALT_COMMAND "/sbin/shutdown -h -P now")
    endif()
    set(REBOOT_COMMAND "/sbin/shutdown -r now")
endif()


# Set constants
set(DATA_INSTALL_DIR            "${CMAKE_INSTALL_FULL_DATADIR}/sddm"                CACHE PATH      "System application data install directory")
set(DBUS_CONFIG_DIR             "${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d"       CACHE PATH      "DBus config files directory")
set(STATE_DIR                   "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/sddm"      CACHE PATH      "State directory")
set(RUNTIME_DIR                 "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/run/sddm"      CACHE PATH      "Runtime data storage directory")
set(QML_INSTALL_DIR             "${QT_IMPORTS_DIR}"                                 CACHE PATH      "QML component installation directory")

set(SESSION_COMMAND             "${DATA_INSTALL_DIR}/scripts/Xsession"              CACHE PATH      "Script to execute when starting the X11 desktop session")
set(WAYLAND_SESSION_COMMAND     "${DATA_INSTALL_DIR}/scripts/wayland-session"       CACHE PATH      "Script to execute when starting the Wayland desktop session")

set(CONFIG_FILE                 "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf"        CACHE PATH      "Path of the sddm config file")
set(CONFIG_DIR                  "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf.d"      CACHE PATH      "Path of the sddm config directory")
set(SYSTEM_CONFIG_DIR           "${CMAKE_INSTALL_PREFIX}/lib/sddm/sddm.conf.d"      CACHE PATH      "Path of the system sddm config directory")
set(LOG_FILE                    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/sddm.log"  CACHE PATH      "Path of the sddm log file")
set(DBUS_CONFIG_FILENAME        "org.freedesktop.DisplayManager.conf"               CACHE STRING    "Name of the sddm config file")
set(COMPONENTS_TRANSLATION_DIR  "${DATA_INSTALL_DIR}/translations"                  CACHE PATH      "Components translations directory")


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

# Add subdirectories
add_subdirectory(components)
add_subdirectory(data)
add_subdirectory(services)
add_subdirectory(src)
add_subdirectory(test)

# Display feature summary
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
