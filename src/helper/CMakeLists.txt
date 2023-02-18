include(CheckLibraryExists)

include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
    ${LIBXAU_INCLUDE_DIRS}
)
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(HELPER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/Auth.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.cpp
    Backend.cpp
    HelperApp.cpp
    UserSession.cpp
)

list(APPEND HELPER_SOURCES ${CMAKE_SOURCE_DIR}/src/common/VirtualTerminal.cpp)

set(HELPER_SOURCES
    ${HELPER_SOURCES}
    backend/PamHandle.cpp
    backend/PamBackend.cpp
)

add_executable(sddm-helper ${HELPER_SOURCES})
target_link_libraries(sddm-helper
                      Qt${QT_MAJOR_VERSION}::Network
                      Qt${QT_MAJOR_VERSION}::DBus
                      Qt${QT_MAJOR_VERSION}::Qml
                      ${LIBXAU_LINK_LIBRARIES})
if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
    # On FreeBSD (possibly other BSDs as well), we want to use
    # setusercontext() to set up the login configuration from login.conf
    find_library(_have_libutil util)
    if(_have_libutil)
        check_library_exists(${_have_libutil} setusercontext "" _have_setusercontext)
    endif()
    if(_have_libutil AND _have_setusercontext)
        target_link_libraries(sddm-helper ${_have_libutil})
    endif()
endif()

if(PAM_FOUND)
    target_link_libraries(sddm-helper ${PAM_LIBRARIES})
else()
    target_link_libraries(sddm-helper crypt)
endif()

install(TARGETS sddm-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

add_executable(sddm-helper-start-wayland HelperStartWayland.cpp waylandsocketwatcher.cpp waylandhelper.cpp ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp)
target_link_libraries(sddm-helper-start-wayland Qt${QT_MAJOR_VERSION}::Core)
install(TARGETS sddm-helper-start-wayland RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

add_executable(sddm-helper-start-x11user HelperStartX11User.cpp xorguserhelper.cpp
                                                ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
                                                ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
                                                ${CMAKE_SOURCE_DIR}/src/common/XAuth.cpp
                                                ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
                                                )
target_link_libraries(sddm-helper-start-x11user Qt${QT_MAJOR_VERSION}::Core
                                                ${LIBXAU_LINK_LIBRARIES})
install(TARGETS sddm-helper-start-x11user RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

if(JOURNALD_FOUND)
    target_link_libraries(sddm-helper ${JOURNALD_LIBRARIES})
    target_link_libraries(sddm-helper-start-x11user ${JOURNALD_LIBRARIES})
    target_link_libraries(sddm-helper-start-wayland ${JOURNALD_LIBRARIES})
endif()
