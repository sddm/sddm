configure_file(${CMAKE_SOURCE_DIR}/data/sddm.conf.in    ${CMAKE_BINARY_DIR}/sddm.conf)

configure_file(themes/circles/Main.qml                  themes/circles/Main.qml)
configure_file(themes/circles/metadata.desktop          themes/circles/metadata.desktop)

configure_file(themes/elarun/Main.qml                   themes/elarun/Main.qml)
configure_file(themes/elarun/metadata.desktop           themes/elarun/metadata.desktop)

configure_file(themes/maldives/Main.qml                 themes/maldives/Main.qml)
configure_file(themes/maldives/metadata.desktop         themes/maldives/metadata.desktop)

configure_file(themes/maui/Main.qml                     themes/maui/Main.qml)
configure_file(themes/maui/metadata.desktop             themes/maui/metadata.desktop)

install(DIRECTORY   faces                               DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY   themes                              DESTINATION ${DATA_INSTALL_DIR})
install(DIRECTORY   ${CMAKE_BINARY_DIR}/data/themes     DESTINATION ${DATA_INSTALL_DIR})

install(FILES       "${CMAKE_BINARY_DIR}/sddm.conf"     DESTINATION ${CONF_INSTALL_DIR})
install(FILES       "${CMAKE_BINARY_DIR}/sddm.conf"     DESTINATION ${DATA_INSTALL_DIR} RENAME sddm.conf.sample)

install(FILES scripts/Xsession DESTINATION ${DATA_INSTALL_DIR}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
