set(COMPONENTS_TRANSLATION_DIR "${DATA_INSTALL_DIR}/translations-qt${QT_MAJOR_VERSION}")
set(COMPONENTS_TRANSLATION_DIR "${COMPONENTS_TRANSLATION_DIR}" PARENT_SCOPE)

set(TRANSLATION_FILES
    ar.ts
    bn.ts
    ca.ts
    cs.ts
    da.ts
    de.ts
    es.ts
    et.ts
    eu.ts
    fi.ts
    fr.ts
    gl.ts
    hi_IN.ts
    he.ts
    hu.ts
    ie.ts
    is.ts
    it.ts
    ja.ts
    ka.ts
    kk.ts
    ko.ts
    lt.ts
    lv.ts
    nb.ts
    nl.ts
    nn.ts
    oc.ts
    pl.ts
    pt_BR.ts
    pt_PT.ts
    ro.ts
    ru.ts
    sk.ts
    sr.ts
    sr@ijekavian.ts
    sr@ijekavianlatin.ts
    sr@latin.ts
    sv.ts
    tr.ts
    uk.ts
    zh_CN.ts
    zh_TW.ts
)

if(QT_MAJOR_VERSION EQUAL "5")
    qt5_add_translation(QM_FILES ${TRANSLATION_FILES})
elseif(QT_MAJOR_VERSION EQUAL "6")
    qt6_add_translation(QM_FILES ${TRANSLATION_FILES})
endif()

install(FILES ${QM_FILES} DESTINATION "${COMPONENTS_TRANSLATION_DIR}")

add_custom_target(components-translation-qt${QT_MAJOR_VERSION} DEPENDS ${QM_FILES})
