set(TRANSLATION_FILES
    ar.ts
    ca.ts
    cs.ts
    da.ts
    de.ts
    es.ts
    et.ts
    fi.ts
    fr.ts
    hi_IN.ts
    hu.ts
    it.ts
    ja.ts
    kk.ts
    ko.ts
    lt.ts
    lv.ts
    nb.ts
    nl.ts
    nn.ts
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

qt5_add_translation(QM_FILES ${TRANSLATION_FILES})

install(FILES ${QM_FILES} DESTINATION "${COMPONENTS_TRANSLATION_DIR}")

add_custom_target(components-translation DEPENDS ${QM_FILES})
