include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
)
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(DAEMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
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
    VirtualTerminal.cpp
)

qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml"          "DisplayManager.h" SDDM::DisplayManager)
qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Seat.xml"     "DisplayManager.h" SDDM::DisplayManagerSeat)
qt5_add_dbus_adaptor(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml"  "DisplayManager.h" SDDM::DisplayManagerSession)

add_executable(sddm ${DAEMON_SOURCES})
target_link_libraries(sddm ${LIBXCB_LIBRARIES})
if(PAM_FOUND)
    target_link_libraries(sddm ${PAM_LIBRARIES})
else()
    target_link_libraries(sddm crypt)
endif()
qt5_use_modules(sddm DBus Network Qml)

if(JOURNALD_FOUND)
    target_link_libraries(sddm ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm DESTINATION "${CMAKE_INSTALL_BINDIR}")
