set(THEMES elarun maldives)

foreach(THEME ${THEMES})
    # We assume that theme-specific translations are stored
    # in ${THEME}/translations directory
    set(TRANSLATIONS_DIR "${THEME}/translations")
    file(GLOB TRANSLATION_SOURCES "${TRANSLATIONS_DIR}/*.ts")

    configure_file("${THEME}/Main.qml"                  "${THEME}/Main.qml")
    configure_file("${THEME}/metadata.desktop"          "${THEME}/metadata.desktop")

    set_source_files_properties(${TRANSLATION_SOURCES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${TRANSLATIONS_DIR}")

    qt5_add_translation(QM_FILES "${TRANSLATION_SOURCES}")

    install(DIRECTORY "${THEME}" DESTINATION "${DATA_INSTALL_DIR}/themes" PATTERN "*.ts" EXCLUDE)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${THEME}" DESTINATION "${DATA_INSTALL_DIR}/themes")

    list(APPEND THEMES_QM_FILES ${QM_FILES})
endforeach(THEME)

add_custom_target(themes-translation DEPENDS ${THEMES_QM_FILES})
