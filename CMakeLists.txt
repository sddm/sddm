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

set(SOURCES
  src/Application.cpp
  src/Authenticator.cpp
  src/Configuration.cpp
  src/Cookie.cpp
  src/DisplayManager.cpp
  src/LockFile.cpp
  src/Main.cpp
  src/SessionManager.cpp
  src/SessionModel.cpp
  src/UserModel.cpp
  src/Util.cpp
)

# PAM
find_package(PAM REQUIRED)

# X11
find_package(X11 REQUIRED)

if (USE_QT5)
  find_package(Qt5Core REQUIRED)

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB})

  qt5_use_modules(sddm Quick)

  # find qt5 imports dir
  get_target_property(QMAKE_EXECUTABLE Qt5::qmake LOCATION)
  exec_program(${QMAKE_EXECUTABLE} ARGS "-query QT_INSTALL_IMPORTS" RETURN_VALUE return_code OUTPUT_VARIABLE QT_IMPORTS_DIR)

  set(COMPONENTS_VERSION 2.0)
else()
  find_package(Qt4 REQUIRED)
  set(QT_USE_QTNETWORK TRUE)
  set(QT_USE_QTDECLARATIVE TRUE)
  include(${QT_USE_FILE})

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})

  set(COMPONENTS_VERSION 1.1)
endif(USE_QT5)

# include pkg-config
find_package(PkgConfig)

# check systemd
pkg_check_modules(SYSTEMD "libsystemd-login libsystemd-daemon" REQUIRED)

# find systemd unit dir
execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
string(REGEX REPLACE "[ \t\n]+" \; SYSTEMD_SYSTEM_UNIT_DIR "${SYSTEMD_SYSTEM_UNIT_DIR}")

# set paths
set(BIN_INSTALL_DIR  ${CMAKE_INSTALL_PREFIX}/bin)
set(DATA_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/share/apps/sddm)

# configure config file
configure_file(${CMAKE_SOURCE_DIR}/sddm.conf.cmake sddm.conf)

#configure themes
configure_file(${CMAKE_SOURCE_DIR}/data/themes/circles/Main.qml          data/themes/circles/Main.qml)
configure_file(${CMAKE_SOURCE_DIR}/data/themes/circles/metadata.desktop  data/themes/circles/metadata.desktop)

configure_file(${CMAKE_SOURCE_DIR}/data/themes/elarun/Main.qml           data/themes/elarun/Main.qml)
configure_file(${CMAKE_SOURCE_DIR}/data/themes/elarun/metadata.desktop   data/themes/elarun/metadata.desktop)

configure_file(${CMAKE_SOURCE_DIR}/data/themes/maldives/Main.qml         data/themes/maldives/Main.qml)
configure_file(${CMAKE_SOURCE_DIR}/data/themes/maldives/metadata.desktop data/themes/maldives/metadata.desktop)

configure_file(${CMAKE_SOURCE_DIR}/data/themes/maui/Main.qml             data/themes/maui/Main.qml)
configure_file(${CMAKE_SOURCE_DIR}/data/themes/maui/metadata.desktop     data/themes/maui/metadata.desktop)

# install binary
install(TARGETS sddm DESTINATION ${BIN_INSTALL_DIR})

# install config file
install(FILES ${CMAKE_BINARY_DIR}/sddm.conf DESTINATION /etc)
install(FILES ${CMAKE_BINARY_DIR}/sddm.conf DESTINATION /etc RENAME sddm.conf.sample)

# install pam service file
install(FILES ${CMAKE_SOURCE_DIR}/services/sddm.pam     DESTINATION /etc/pam.d RENAME sddm)

# install systemd service file
if(SYSTEMD_SYSTEM_UNIT_DIR)
    install(FILES ${CMAKE_SOURCE_DIR}/services/sddm.systemd DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR} RENAME sddm.service)
endif(SYSTEMD_SYSTEM_UNIT_DIR)

# install components
if(USE_QT5)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/components/2.0/ DESTINATION ${QT_IMPORTS_DIR}/SddmComponents)
else()
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/components/1.1/ DESTINATION ${QT_IMPORTS_DIR}/SddmComponents)
endif(USE_QT5)

# install data files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/faces  DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/themes DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY ${CMAKE_BINARY_DIR}/data/themes DESTINATION ${DATA_INSTALL_DIR})

# install scripts
install(FILES ${CMAKE_SOURCE_DIR}/data/scripts/Xsession
        DESTINATION ${DATA_INSTALL_DIR}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
