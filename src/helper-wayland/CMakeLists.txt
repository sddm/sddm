include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
)

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.h
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.h
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.h
    main.cpp
    waylandsocketwatcher.cpp
    waylandsocketwatcher.h
    waylanduserhelper.cpp
    waylanduserhelper.h
)
add_executable(sddm-helper-wayland ${SOURCES})
target_link_libraries(sddm-helper-wayland Qt5::Core)
install(TARGETS sddm-helper-wayland RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
