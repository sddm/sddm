if(USE_SYSTEMD)
	install(FILES sddm.systemd DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR} RENAME sddm.service)
endif()

install(FILES sddm.pam DESTINATION /etc/pam.d RENAME sddm)
