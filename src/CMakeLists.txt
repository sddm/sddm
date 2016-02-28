configure_file("common/Constants.h.in" "common/Constants.h" IMMEDIATE @ONLY)

if(ENABLE_WAYLAND)
    add_subdirectory(compositor)
endif()
add_subdirectory(daemon)
add_subdirectory(greeter)
add_subdirectory(helper)
