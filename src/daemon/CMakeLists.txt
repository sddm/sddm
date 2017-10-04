include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
    "${CMAKE_BINARY_DIR}/src/common"
    "${LIBXCB_INCLUDE_DIR}"
)

set(DAEMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/Auth.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.cpp

    DaemonApp.cpp
    Display.cpp
    DisplayManager.cpp
    DisplayServer.cpp
    LogindDBusTypes.cpp
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


set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Manager.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)
set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Seat.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)

set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Session.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)

qt5_add_dbus_interface(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Manager.xml"  "Login1Manager")
qt5_add_dbus_interface(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Seat.xml"  "Login1Seat")
qt5_add_dbus_interface(DAEMON_SOURCES "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Session.xml"  "Login1Session")

add_executable(sddm ${DAEMON_SOURCES})
target_link_libraries(sddm
                      Qt5::DBus
                      Qt5::Network
                      Qt5::Qml
                      ${LIBXCB_LIBRARIES})
if(PAM_FOUND)
    target_link_libraries(sddm ${PAM_LIBRARIES})
else()
    target_link_libraries(sddm crypt)
endif()

if(JOURNALD_FOUND)
    target_link_libraries(sddm ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Generate and install the default config file
add_custom_command(COMMAND sddm --example-config > "${CMAKE_BINARY_DIR}/sddm.conf"
    OUTPUT "${CMAKE_BINARY_DIR}/sddm.conf"
    COMMENT "Generate default sddm.conf file"
    DEPENDS sddm
)
add_custom_target(generate_config_file ALL DEPENDS "${CMAKE_BINARY_DIR}/sddm.conf")
install(FILES "${CMAKE_BINARY_DIR}/sddm.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
