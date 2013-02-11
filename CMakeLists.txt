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

  set(THEME Maldives)
else()
  find_package(Qt4 REQUIRED)
  set(QT_USE_QTNETWORK TRUE)
  set(QT_USE_QTDECLARATIVE TRUE)
  include(${QT_USE_FILE})

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})

  set(THEME Circles)
endif(USE_QT5)

# set paths
set(BIN_INSTALL_DIR  ${CMAKE_INSTALL_PREFIX}/bin)
set(DATA_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/share/apps/sddm)

# replace variables in the config file
configure_file(${CMAKE_SOURCE_DIR}/sddm.conf.cmake sddm.conf)

# install binary
install(TARGETS sddm DESTINATION ${BIN_INSTALL_DIR})

# install config file
install(FILES ${CMAKE_BINARY_DIR}/sddm.conf DESTINATION /etc)
install(FILES ${CMAKE_BINARY_DIR}/sddm.conf DESTINATION /etc RENAME sddm.conf.sample)

# install service files
install(FILES ${CMAKE_SOURCE_DIR}/services/sddm.pam     DESTINATION /etc/pam.d RENAME sddm)
install(FILES ${CMAKE_SOURCE_DIR}/services/sddm.systemd DESTINATION /usr/lib/systemd/system RENAME sddm.service)

# install data files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/faces  DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/themes DESTINATION ${DATA_INSTALL_DIR})

# install scripts
install(FILES ${CMAKE_SOURCE_DIR}/data/scripts/Xsession
        DESTINATION ${DATA_INSTALL_DIR}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
