set(THEMES elarun maldives maya)

foreach(THEME ${THEMES})
    # We assume that theme-specific translations are stored
    # in ${THEME}/translations directory
    set(TRANSLATIONS_DIR "${THEME}/translations")
    file(GLOB TRANSLATION_SOURCES "${TRANSLATIONS_DIR}/*.ts")

    set_source_files_properties(${TRANSLATION_SOURCES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${TRANSLATIONS_DIR}")

    qt_add_translation(QM_FILES "${TRANSLATION_SOURCES}")

    configure_file("${THEME}/metadata.desktop.in" "${THEME}/metadata.desktop" @ONLY)

    install(DIRECTORY "${THEME}" DESTINATION "${DATA_INSTALL_DIR}/themes"
            PATTERN "${THEME}/*.in" EXCLUDE
            PATTERN "${THEME}/*.ts" EXCLUDE
            PATTERN "${THEME}/.gitattributes" EXCLUDE)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${THEME}/metadata.desktop"
            DESTINATION "${DATA_INSTALL_DIR}/themes/${THEME}/")

    list(APPEND THEMES_QM_FILES ${QM_FILES})
endforeach(THEME)

add_custom_target(themes-translation DEPENDS ${THEMES_QM_FILES})
