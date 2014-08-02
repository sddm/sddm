find_program(RST2MAN_EXECUTABLE NAMES rst2man rst2man.py rst2man2 rst2man2.py)
if(NOT RST2MAN_EXECUTABLE)
    message(FATAL "rst2man (python-docutils) is required to build man pages")
endif()

function(BUILD_MAN_PAGE _sources _src _dst)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_src}.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${_src}"
                   IMMEDIATE @ONLY)

    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_dst}"
                       COMMAND ${RST2MAN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/${_src}" "${CMAKE_CURRENT_BINARY_DIR}/${_dst}"
                       DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_src}")

    list(APPEND "${_sources}" "${CMAKE_CURRENT_BINARY_DIR}/${_dst}")
    set("${_sources}" "${${_sources}}" PARENT_SCOPE)

    add_custom_target("man_${_src}" ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_dst}")
endfunction()

build_man_page(MAN1_OUTPUT "sddm.rst" "sddm.1")
build_man_page(MAN1_OUTPUT "sddm-greeter.rst" "sddm-greeter.1")
build_man_page(MAN5_OUTPUT "sddm.conf.rst" "sddm.conf.5")
build_man_page(MAN5_OUTPUT "sddm-state.conf.rst" "sddm-state.conf.5")

install(FILES ${MAN1_OUTPUT} DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man1")
install(FILES ${MAN5_OUTPUT} DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man5")
