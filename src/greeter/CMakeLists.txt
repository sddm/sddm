include_directories("${CMAKE_SOURCE_DIR}/src/common")
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(GREETER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    GreeterApp.cpp
    GreeterProxy.cpp
    KeyboardLayout.cpp
    KeyboardModel.cpp
    ScreenModel.cpp
    SessionModel.cpp
    ThemeConfig.cpp
    ThemeMetadata.cpp
    UserModel.cpp
    XcbKeyboardBackend.cpp
)

configure_file("theme/Main.qml"                  "theme/Main.qml")
configure_file("theme.qrc"                       "theme.qrc")

qt5_add_resources(RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)

add_executable(sddm-greeter ${GREETER_SOURCES} ${RESOURCES})
target_link_libraries(sddm-greeter ${LIBXCB_LIBRARIES} ${LIBXKB_LIBRARIES})
qt5_use_modules(sddm-greeter Quick)

if(JOURNALD_FOUND)
    target_link_libraries(sddm-greeter ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(sddm-greeter components-translation)
add_dependencies(sddm-greeter themes-translation)

install(TARGETS sddm-greeter DESTINATION "${CMAKE_INSTALL_BINDIR}")
