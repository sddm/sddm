include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
    "${LIBXCB_INCLUDE_DIR}"
)

set(GREETER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.cpp
    ${CMAKE_SOURCE_DIR}/src/common/PamTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.cpp
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

qt5_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)

add_executable(sddm-greeter ${GREETER_SOURCES} ${RESOURCES})
target_link_libraries(sddm-greeter
                      Qt5::Quick
                      ${LIBXCB_LIBRARIES}
                      ${LIBXKB_LIBRARIES})

if(JOURNALD_FOUND)
    target_link_libraries(sddm-greeter ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(sddm-greeter components-translation)
add_dependencies(sddm-greeter themes-translation)

install(TARGETS sddm-greeter DESTINATION "${CMAKE_INSTALL_BINDIR}")
