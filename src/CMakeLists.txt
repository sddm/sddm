include_directories("common")
include_directories("${CMAKE_BINARY_DIR}/src/common")

configure_file("common/Constants.h.in" "common/Constants.h" IMMEDIATE @ONLY)

## DAEMON ##

set(DAEMON_SOURCES
    common/Configuration.cpp
    common/SocketWriter.cpp
    daemon/Authenticator.cpp
    daemon/DaemonApp.cpp
    daemon/Display.cpp
    daemon/DisplayManager.cpp
    daemon/DisplayServer.cpp
    daemon/Greeter.cpp
    daemon/PowerManager.cpp
    daemon/Seat.cpp
    daemon/SeatManager.cpp
    daemon/Session.cpp
    daemon/SignalHandler.cpp
    daemon/SocketServer.cpp
)

if(PAM_FOUND)
    set(DAEMON_SOURCES
        ${DAEMON_SOURCES}
        daemon/PamService.cpp
    )
endif()

if(USE_QT5)
    qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml"          "daemon/DisplayManager.h" SDDM::DisplayManager)
    qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Seat.xml"     "daemon/DisplayManager.h" SDDM::DisplayManagerSeat)
    qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml"  "daemon/DisplayManager.h" SDDM::DisplayManagerSession)

    add_executable(sddm ${DAEMON_SOURCES})
    target_link_libraries(sddm ${LIBXCB_LIBRARIES})
    if(PAM_FOUND)
      target_link_libraries(sddm ${PAM_LIBRARIES})
    else()
      target_link_libraries(sddm crypt)
    endif()
    qt5_use_modules(sddm DBus Network)
else()
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTDBUS TRUE)
    include(${QT_USE_FILE})

    qt4_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml"          daemon/DisplayManager.h SDDM::DisplayManager)
    qt4_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Seat.xml"     daemon/DisplayManager.h SDDM::DisplayManagerSeat)
    qt4_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml"  daemon/DisplayManager.h SDDM::DisplayManagerSession)

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

## GREETER ##

set(GREETER_SOURCES
    common/Configuration.cpp
    common/SocketWriter.cpp
    greeter/GreeterApp.cpp
    greeter/GreeterProxy.cpp
    greeter/KeyboardModel.cpp
    greeter/ScreenModel.cpp
    greeter/SessionModel.cpp
    greeter/ThemeConfig.cpp
    greeter/ThemeMetadata.cpp
    greeter/UserModel.cpp
)

if(USE_QT5)
    add_executable(sddm-greeter ${GREETER_SOURCES})
    target_link_libraries(sddm-greeter ${LIBXCB_LIBRARIES} ${LIBXKB_LIBRARIES})
    qt5_use_modules(sddm-greeter Quick)
else()
    set(QT_USE_QTDECLARATIVE TRUE)
    include(${QT_USE_FILE})

    add_executable(sddm-greeter ${GREETER_SOURCES})
    target_link_libraries(sddm-greeter ${LIBXCB_LIBRARIES} ${LIBXKB_LIBRARIES} ${QT_LIBRARIES})
endif()

if(JOURNALD_FOUND)
    target_link_libraries(sddm-greeter ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(sddm-greeter components-translation)
add_dependencies(sddm-greeter themes-translation)

install(TARGETS sddm-greeter DESTINATION "${CMAKE_INSTALL_BINDIR}")
