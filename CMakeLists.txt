cmake_minimum_required(VERSION 2.8.8)

project(SDDM)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_definitions(-Wall -march=native -O2 -g -std=c++11)

set(SOURCES
  src/Authenticator.cpp
  src/Configuration.cpp
  src/Cookie.cpp
  src/DisplayManager.cpp
  src/LockFile.cpp
  src/Main.cpp
  src/SessionManager.cpp
)

# PAM
find_package(PAM REQUIRED)

# X11
find_package(X11 REQUIRED)

if (USE_QT5)
  find_package(Qt5Core REQUIRED)

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB})

  qt5_use_modules(sddm Widgets Quick)
else()
  find_package(Qt4 REQUIRED)
  set(QT_USE_QTNETWORK TRUE)
  set(QT_USE_QTDECLARATIVE TRUE)
  include(${QT_USE_FILE})

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})
endif(USE_QT5)
