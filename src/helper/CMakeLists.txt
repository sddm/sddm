include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
)
include_directories("${CMAKE_BINARY_DIR}/src/common")

set(HELPER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    Backend.cpp
    HelperApp.cpp
    UserSession.cpp
)

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
if(PAM_FOUND)
    target_link_libraries(sddm-helper ${PAM_LIBRARIES})
else()
    target_link_libraries(sddm-helper crypt)
endif()

if(JOURNALD_FOUND)
    target_link_libraries(sddm-helper ${JOURNALD_LIBRARIES})
endif()

install(TARGETS sddm-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
