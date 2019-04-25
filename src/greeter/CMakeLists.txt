include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
)

if(NOT BUILD_EMBEDDED)
    include_directories("${LIBXCB_INCLUDE_DIR}")
endif()

set(GREETER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
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
)

if(NOT BUILD_EMBEDDED)
    set(GREETER_SOURCES
        ${GREETER_SOURCES}
        XcbKeyboardBackend.cpp
       )
else()
    set(GREETER_SOURCES
        ${GREETER_SOURCES}
        EmbeddedKeyboardBackend.cpp
       )
endif()

if(NOT BUILD_EMBEDDED)
configure_file("theme.qrc" "theme.qrc")
qt5_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)
else()
configure_file("theme-embedded.qrc" "theme-embedded.qrc")
qt5_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme-embedded.qrc)
endif()

add_executable(sddm-greeter ${GREETER_SOURCES} ${RESOURCES})
target_link_libraries(sddm-greeter
                      Qt5::Quick)

if(NOT BUILD_EMBEDDED)
    target_link_libraries(sddm-greeter
                          ${LIBXCB_LIBRARIES}
                          ${LIBXKB_LIBRARIES})
endif()

if(JOURNALD_FOUND)
    target_link_libraries(sddm-greeter ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(sddm-greeter components-translation)
add_dependencies(sddm-greeter themes-translation)

install(TARGETS sddm-greeter DESTINATION "${CMAKE_INSTALL_BINDIR}")
