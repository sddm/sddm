if(SYSTEMD_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sddm.service.in" "${CMAKE_CURRENT_BINARY_DIR}/sddm.service")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sddm.service" DESTINATION "${SYSTEMD_SYSTEM_UNIT_DIR}")
endif()

install(FILES sddm.pam DESTINATION /etc/pam.d RENAME sddm)
install(FILES sddm-autologin.pam DESTINATION /etc/pam.d RENAME sddm-autologin)
install(FILES sddm-greeter.pam DESTINATION /etc/pam.d RENAME sddm-greeter)
