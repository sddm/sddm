include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/auth"
)

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.h
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.h
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.h
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.cpp
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.h
    main.cpp
    xorguserhelper.cpp
    xorguserhelper.h
)
add_executable(sddm-helper-x11 ${SOURCES})
target_link_libraries(sddm-helper-x11 Qt5::Core)
install(TARGETS sddm-helper-x11 RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
