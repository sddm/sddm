include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
    "${LIBXCB_INCLUDE_DIR}"
)

set(GREETER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.cpp
    GreeterApp.cpp
    GreeterProxy.cpp
    KeyboardLayout.cpp
    KeyboardModel.cpp
    ScreenModel.cpp
    SessionModel.cpp
    UserModel.cpp
    waylandkeyboardbackend.cpp
    waylandkeyboardbackend.h
    XcbKeyboardBackend.cpp
)

configure_file("theme.qrc" "theme.qrc")
configure_file("theme/metadata.desktop.in" "theme/metadata.desktop" @ONLY)

qt_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)

# The unversioned name was used for the old Qt5 version.
add_executable(sddm-greeter-qt6 ${GREETER_SOURCES} ${RESOURCES})
target_link_libraries(sddm-greeter-qt6
                      Qt6::Quick
                      ${LIBXCB_LIBRARIES}
                      ${LIBXKB_LIBRARIES})

if(JOURNALD_FOUND)
    target_link_libraries(sddm-greeter-qt6 ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(sddm-greeter-qt6 components-translation themes-translation)

install(TARGETS sddm-greeter-qt6 DESTINATION "${CMAKE_INSTALL_BINDIR}")
