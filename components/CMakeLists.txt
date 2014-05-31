if(USE_QT5)
    set(READONLY_KEYWORD readonly)
endif()

configure_file("${COMPONENTS_VERSION}/LayoutBox.qml" "${COMPONENTS_VERSION}/LayoutBox.qml")
configure_file("common/TextConstants.qml" "common/TextConstants.qml")
configure_file("common/qmldir" "common/qmldir")

install(DIRECTORY "${COMPONENTS_VERSION}/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${COMPONENTS_VERSION}/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")

install(DIRECTORY "common/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/common/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
