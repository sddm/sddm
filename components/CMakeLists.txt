configure_file("2.0/LayoutBox.qml" "2.0/LayoutBox.qml")

install(DIRECTORY "2.0/" DESTINATION "${QML_INSTALL_DIR}/SddmComponents" PATTERN "LayoutBox.qml" EXCLUDE)
install(DIRECTORY "common/" DESTINATION "${QML_INSTALL_DIR}/SddmComponents")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/2.0/" DESTINATION "${QML_INSTALL_DIR}/SddmComponents")
