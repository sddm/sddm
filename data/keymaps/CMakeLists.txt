find_program(GZIP_EXECUTABLE gzip)
if(NOT GZIP_EXECUTABLE)
    message(FATAL_ERROR "gzip is required to create keymaps")
endif()

find_program(KMAP2QMAP_EXECUTABLE kmap2qmap)
if(NOT KMAP2QMAP_EXECUTABLE)
    message(FATAL_ERROR "kmap2qmap is required to create keymaps")
endif()

set(KEYMAPS
#    ar_AR
    bg_BG
    cs_CZ
    da_DK
    de_DE
    el_GR
    en_GB
    es_ES
    et_EE
#    fa_FA
    fi_FI
    fr_FR
    he_IL
    hi_IN
    hr_HR
    hu_HU
    it_IT
    ja_JP
    ko_KR
    nb_NO
    nl_NL
    pl_PL
    pt_PT
    ro_RO
    ru_RU
    sr_SP
    sv_SE
    zh_CN
    zh_TW
)

foreach(KEYMAP ${KEYMAPS})
    execute_process(COMMAND ${GZIP_EXECUTABLE} -dc ${CMAKE_CURRENT_SOURCE_DIR}/${KEYMAP}.map.gz
                    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.map)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.qmap"
                       COMMAND ${KMAP2QMAP_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.map" "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.qmap"
                       DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.map")
    list(APPEND QMAP_FILES "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.qmap")
    add_custom_target("map_${KEYMAP}" ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${KEYMAP}.qmap")
endforeach(KEYMAP)

install(FILES ${QMAP_FILES} DESTINATION "${DATA_INSTALL_DIR}/keymaps")
