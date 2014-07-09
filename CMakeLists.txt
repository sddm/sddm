cmake_minimum_required(VERSION 2.8.8)

project(SDDM)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Silence CMake 3.0 warnings
if(POLICY CMP0043)
  cmake_policy(SET CMP0043 OLD)
endif()

# Set version
set(SDDM_VERSION_MAJOR 0)
set(SDDM_VERSION_MINOR 1)
set(SDDM_VERSION_PATCH 0)
set(SDDM_VERSION_STRING "${SDDM_VERSION_MAJOR}.${SDDM_VERSION_MINOR}.${SDDM_VERSION_PATCH}")

option(BUILD_MAN_PAGES "Build man pages" OFF)
option(ENABLE_JOURNALD "Enable logging to journald" ON)

add_definitions(-Wall -std=c++11)

if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
endif()

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

# PKG-CONFIG
find_package(PkgConfig)

# PAM
if(NOT NO_PAM)
  find_package(PAM)

  if(PAM_FOUND)
    add_definitions(-DUSE_PAM)
  endif()
endif()

# XCB
find_package(XCB REQUIRED)

# XKB
find_package(XKB REQUIRED)

if(USE_QT5)
  find_package(Qt5Core REQUIRED)
  find_package(Qt5DBus REQUIRED)
  find_package(Qt5LinguistTools REQUIRED)

  # find qt5 imports dir
  get_target_property(QMAKE_EXECUTABLE Qt5::qmake LOCATION)
  exec_program(${QMAKE_EXECUTABLE} ARGS "-query QT_INSTALL_QML" RETURN_VALUE return_code OUTPUT_VARIABLE QT_IMPORTS_DIR)

  add_definitions(-DUSE_QT5)
  set(CMAKE_AUTOMOC_MOC_OPTIONS -DUSE_QT5)

  set(COMPONENTS_VERSION 2.0)
else()
  find_package(Qt4 REQUIRED)

  # find qt4 imports dir, required if no QML plugins are already installed
  # for example, see: https://bugs.launchpad.net/ubuntu/+source/cmake/+bug/894805
  # and many other bug reports
  _qt4_query_qmake(QT_INSTALL_IMPORTS QT_IMPORTS_DIR)
  set(QT_IMPORTS_DIR "${QT_IMPORTS_DIR}")

  set(COMPONENTS_VERSION 1.1)
endif()

# uninstall target
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
                                                         "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
                                                         IMMEDIATE @ONLY)

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# systemd
if(NOT NO_SYSTEMD)
    pkg_check_modules(SYSTEMD "systemd")
endif()

if(SYSTEMD_FOUND)
    pkg_check_modules(JOURNALD "libsystemd-journal")

    if(ENABLE_JOURNALD)
        if(JOURNALD_FOUND)
            add_definitions(-DHAVE_JOURNALD)
            set(CMAKE_AUTOMOC_MOC_OPTIONS -DHAVE_JOURNALD)
        else()
            message(WARNING "Disable journald support for lack of libsystemd-journal")
        endif()
    endif()

    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
    string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SYSTEM_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR})
    set(MINIMUM_VT 1)
    set(HALT_COMMAND "/usr/bin/systemctl poweroff")
    set(REBOOT_COMMAND "/usr/bin/systemctl reboot")
else()
    set(SYSTEMD_FOUND 0)
    set(MINIMUM_VT 7)
    set(HALT_COMMAND "/sbin/shutdown -h -P now")
    set(REBOOT_COMMAND "/sbin/shutdown -r now")
endif()

# set constants
set(DATA_INSTALL_DIR            "${CMAKE_INSTALL_FULL_DATADIR}/sddm"                CACHE PATH      "System application data install directory")
set(DBUS_CONFIG_DIR             "${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d"       CACHE PATH      "DBus config files directory")
set(STATE_DIR                   "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/sddm"      CACHE PATH      "State directory")

set(SESSION_COMMAND             "${DATA_INSTALL_DIR}/scripts/Xsession"              CACHE PATH      "Script to execute when starting the desktop session")

set(CONFIG_FILE                 "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf"        CACHE PATH      "Path of the sddm config file")
set(LOG_FILE                    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/sddm.log"  CACHE PATH      "Path of the sddm log file")
set(COMPONENTS_TRANSLATION_DIR  "${DATA_INSTALL_DIR}/translations"                  CACHE PATH      "Components translations directory")

add_subdirectory(components)
add_subdirectory(data)
add_subdirectory(services)
add_subdirectory(src)
