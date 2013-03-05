cmake_minimum_required(VERSION 2.8.8)

project(SDDM)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_definitions(-Wall -march=native -std=c++11)

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

# PAM
find_package(PAM REQUIRED)

# X11
find_package(X11 REQUIRED)

if(USE_QT5)
  find_package(Qt5Core REQUIRED)

  # find qt5 imports dir
  get_target_property(QMAKE_EXECUTABLE Qt5::qmake LOCATION)
  exec_program(${QMAKE_EXECUTABLE} ARGS "-query QT_INSTALL_QML" RETURN_VALUE return_code OUTPUT_VARIABLE QT_IMPORTS_DIR)

  set(COMPONENTS_VERSION 2.0)
else()
  find_package(Qt4 REQUIRED)

  set(COMPONENTS_VERSION 1.1)
endif()

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

if(USE_SYSTEMD)
    # include pkg-config
    find_package(PkgConfig)

    # check systemd
    pkg_check_modules(SYSTEMD "libsystemd-login libsystemd-daemon" REQUIRED)

    # find systemd unit dir
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
    string(REGEX REPLACE "[ \t\n]+" \; SYSTEMD_SYSTEM_UNIT_DIR "${SYSTEMD_SYSTEM_UNIT_DIR}")

    add_definitions(-DUSE_SYSTEMD)
endif()

# set constants
set(BIN_INSTALL_DIR     "${CMAKE_INSTALL_PREFIX}/bin"               CACHE PATH      "System binary install directory")
set(DATA_INSTALL_DIR    "${CMAKE_INSTALL_PREFIX}/share/apps/sddm"   CACHE PATH      "System application data install directory")
set(CONF_INSTALL_DIR    "/etc"                                      CACHE PATH      "System config directory")

set(PASSWD_FILE         "${CONF_INSTALL_DIR}/passwd"                CACHE PATH      "Path of the passwd file")
set(CONFIG_FILE         "${CONF_INSTALL_DIR}/sddm.conf"             CACHE PATH      "Path of the sddm config file")

# configure config file
configure_file(${CMAKE_SOURCE_DIR}/sddm.conf.cmake sddm.conf)

# install config file
install(FILES           "${CMAKE_BINARY_DIR}/sddm.conf"             DESTINATION ${CONF_INSTALL_DIR})
install(FILES           "${CMAKE_BINARY_DIR}/sddm.conf"             DESTINATION ${CONF_INSTALL_DIR} RENAME sddm.conf.sample)

add_subdirectory(components)
add_subdirectory(data)
add_subdirectory(services)
add_subdirectory(src)
