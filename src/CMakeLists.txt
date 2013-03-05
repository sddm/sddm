include_directories(common)
include_directories(${CMAKE_BINARY_DIR}/src/common)

configure_file(common/Constants.h.in common/Constants.h IMMEDIATE @ONLY)

## DAEMON ##

set(DAEMON_SOURCES
    common/Configuration.cpp
    common/SocketWriter.cpp
    daemon/Authenticator.cpp
    daemon/DaemonApp.cpp
    daemon/Display.cpp
    daemon/DisplayServer.cpp
    daemon/Greeter.cpp
    daemon/PowerManager.cpp
    daemon/SessionProcess.cpp
    daemon/SignalHandler.cpp
    daemon/SocketServer.cpp
)

if(USE_QT5)
    add_executable(sddm ${DAEMON_SOURCES})
    target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB})
    qt5_use_modules(sddm DBus Network)
else()
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTDBUS TRUE)
    include(${QT_USE_FILE})

    add_executable(sddm ${DAEMON_SOURCES})
    target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})
endif()

install(TARGETS sddm DESTINATION ${BIN_INSTALL_DIR})

## GREETER ##

set(GREETER_SOURCES
    common/Configuration.cpp
    common/SocketWriter.cpp
    greeter/GreeterApp.cpp
    greeter/GreeterProxy.cpp
    greeter/ScreenModel.cpp
    greeter/SessionModel.cpp
    greeter/ThemeConfig.cpp
    greeter/ThemeMetadata.cpp
    greeter/UserModel.cpp
)

if(USE_QT5)
    add_executable(sddm-greeter ${GREETER_SOURCES})
    target_link_libraries(sddm-greeter)
    qt5_use_modules(sddm-greeter Quick)
else()
    set(QT_USE_QTDECLARATIVE TRUE)
    include(${QT_USE_FILE})

    add_executable(sddm-greeter ${GREETER_SOURCES})
    target_link_libraries(sddm-greeter ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})
endif()

install(TARGETS sddm-greeter DESTINATION ${BIN_INSTALL_DIR})
