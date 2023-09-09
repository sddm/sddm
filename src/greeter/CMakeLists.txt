if(QT_MAJOR_VERSION EQUAL "5")
    # Keep the unversioned name for Qt5. When upgrading SDDM, the old daemon
    # might still be running and only know about "sddm-greeter". Keeping the
    # previous name around also helps users calling it directly.
    set(GREETER_TARGET sddm-greeter)
else()
    set(GREETER_TARGET sddm-greeter-qt${QT_MAJOR_VERSION})
endif()

message(STATUS "Building greeter for Qt ${QT_MAJOR_VERSION} as ${GREETER_TARGET}")

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

qt_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)

add_executable(${GREETER_TARGET} ${GREETER_SOURCES} ${RESOURCES})
target_link_libraries(${GREETER_TARGET}
                      Qt${QT_MAJOR_VERSION}::Quick
                      ${LIBXCB_LIBRARIES}
                      ${LIBXKB_LIBRARIES})

if(JOURNALD_FOUND)
    target_link_libraries(${GREETER_TARGET} ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(${GREETER_TARGET} components-translation themes-translation)

install(TARGETS ${GREETER_TARGET} DESTINATION "${CMAKE_INSTALL_BINDIR}")
