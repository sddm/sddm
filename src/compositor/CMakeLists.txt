set(COMPOSITOR_SOURCES
    Main.cpp
)

qt5_add_resources(COMPOSITOR_RESOURCES compositor.qrc)

add_executable(sddm-wayland-compositor ${COMPOSITOR_SOURCES} ${COMPOSITOR_RESOURCES})
target_link_libraries(sddm-wayland-compositor Qt5::Qml Qt5::Quick GreenIsland::Server)

install(TARGETS sddm-wayland-compositor DESTINATION "${CMAKE_INSTALL_BINDIR}")
