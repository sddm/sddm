include_directories("${CMAKE_SOURCE_DIR}/src/common")
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon/SignalHandler.cpp
    main.cpp
    greeter.cpp
    launcher.cpp
    process.cpp
    usersession.cpp
    xorgserver.cpp
)

add_executable(sddm-display ${SOURCES})
target_link_libraries(sddm-display Qt5::Core)

if(JOURNALD_FOUND)
    target_link_libraries(sddm-display ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm-display DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
