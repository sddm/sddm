if(SYSTEMD_FOUND)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sddm.systemd.in ${CMAKE_CURRENT_BINARY_DIR}/sddm.systemd)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sddm.systemd DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR} RENAME sddm.service)
endif()

install(FILES sddm.pam DESTINATION /etc/pam.d RENAME sddm)
install(FILES sddm-autologin.pam DESTINATION /etc/pam.d RENAME sddm-autologin)
