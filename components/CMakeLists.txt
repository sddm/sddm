configure_file("2.0/LayoutBox.qml" "2.0/LayoutBox.qml")

install(DIRECTORY "2.0/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
install(DIRECTORY "common/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/2.0/" DESTINATION "${QT_IMPORTS_DIR}/SddmComponents")
