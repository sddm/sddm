include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
)
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(DAEMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/Auth.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.cpp
    DaemonApp.cpp
    Display.cpp
    DisplayManager.cpp
    DisplayServer.cpp
    XorgDisplayServer.cpp
    Greeter.cpp
    PowerManager.cpp
    Seat.cpp
    SeatManager.cpp
    SignalHandler.cpp
    SocketServer.cpp
    Utils.cpp
)

qt4_add_dbus_adaptor(DAEMON_SOURCES
                     "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml"
                     "${CMAKE_CURRENT_SOURCE_DIR}/DisplayManager.h"
                     SDDM::DisplayManager)
qt4_add_dbus_adaptor(DAEMON_SOURCES
                     "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Seat.xml"
                     "${CMAKE_CURRENT_SOURCE_DIR}/DisplayManager.h"
                     SDDM::DisplayManagerSeat)
qt4_add_dbus_adaptor(DAEMON_SOURCES
                     "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml"
                     "${CMAKE_CURRENT_SOURCE_DIR}/DisplayManager.h"
                     SDDM::DisplayManagerSession)

if(USE_QT5)
    add_executable(sddm ${DAEMON_SOURCES})
    target_link_libraries(sddm ${LIBXCB_LIBRARIES})
    if(PAM_FOUND)
      target_link_libraries(sddm ${PAM_LIBRARIES})
    else()
      target_link_libraries(sddm crypt)
    endif()
    qt5_use_modules(sddm DBus Network Qml)
else()
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTDBUS TRUE)
    set(QT_USE_QTDECLARATIVE TRUE)
    include(${QT_USE_FILE})

    add_executable(sddm ${DAEMON_SOURCES})
    target_link_libraries(sddm ${LIBXCB_LIBRARIES} ${QT_LIBRARIES})
    if(PAM_FOUND)
      target_link_libraries(sddm ${PAM_LIBRARIES})
    else()
      target_link_libraries(sddm crypt)
    endif()
endif()

if(JOURNALD_FOUND)
    target_link_libraries(sddm ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm DESTINATION "${CMAKE_INSTALL_BINDIR}")
