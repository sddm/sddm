include(CheckLibraryExists)

include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
)
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(HELPER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.cpp
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.h
    ${CMAKE_SOURCE_DIR}/src/common/Utils.cpp
    Backend.cpp
    HelperApp.cpp
    UserSession.cpp
    xorguserhelper.cpp
    xorguserhelper.h
    waylandhelper.cpp
    waylandsocketwatcher.cpp
)

# Different implementations of the VT switching code
# (where the FreeBSD version does nothing).
if(${CMAKE_SYSTEM} MATCHES "FreeBSD")
    list(APPEND HELPER_SOURCES ${CMAKE_SOURCE_DIR}/src/common/VirtualTerminal_FreeBSD.cpp)
else()
    list(APPEND HELPER_SOURCES ${CMAKE_SOURCE_DIR}/src/common/VirtualTerminal.cpp)
endif()

if(PAM_FOUND)
    set(HELPER_SOURCES
        ${HELPER_SOURCES}
        backend/PamHandle.cpp
        backend/PamBackend.cpp
    )
else()
    set(HELPER_SOURCES
        ${HELPER_SOURCES}
        backend/PasswdBackend.cpp
    )

   if(HAVE_GETSPNAM)
       add_definitions(-DHAVE_GETSPNAM=1)
   endif()
endif()

add_executable(sddm-helper ${HELPER_SOURCES})
target_link_libraries(sddm-helper Qt5::Network Qt5::DBus Qt5::Qml)
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

if(JOURNALD_FOUND)
    target_link_libraries(sddm-helper ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
