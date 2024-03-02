if(SYSTEMD_FOUND)
    configure_file(sddm.service.in sddm.service)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sddm.service" DESTINATION "${SYSTEMD_SYSTEM_UNIT_DIR}")

    configure_file(sddm-sysuser.conf.in sddm-sysuser.conf)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sddm-sysuser.conf" DESTINATION "${SYSTEMD_SYSUSERS_DIR}" RENAME sddm.conf)
endif()

# systemd-tmpfiles can be used standalone without other systemd parts
if(DEFINED SYSTEMD_TMPFILES_DIR)
    configure_file(sddm-tmpfiles.conf.in sddm-tmpfiles.conf)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sddm-tmpfiles.conf" DESTINATION "${SYSTEMD_TMPFILES_DIR}" RENAME sddm.conf)
endif()
