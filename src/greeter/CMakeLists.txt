include_directories("${CMAKE_SOURCE_DIR}/src/common")
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(GREETER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    GreeterApp.cpp
    GreeterProxy.cpp
    KeyboardModel.cpp
    ScreenModel.cpp
    SessionModel.cpp
    ThemeConfig.cpp
    ThemeMetadata.cpp
    UserModel.cpp
)

if(USE_QT5)
    add_executable(sddm-greeter ${GREETER_SOURCES})
    target_link_libraries(sddm-greeter ${LIBXCB_LIBRARIES} ${LIBXKB_LIBRARIES})
    qt5_use_modules(sddm-greeter Quick)
else()
    set(QT_USE_QTNETWORK TRUE)
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
