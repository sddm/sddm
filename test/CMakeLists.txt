set(QT_USE_QTTEST TRUE)

include_directories(../src/common)

set(ConfigurationTest_SRCS ConfigurationTest.cpp ../src/common/ConfigReader.cpp)
add_executable(ConfigurationTest${QT_MAJOR_VERSION} ${ConfigurationTest_SRCS})
add_test(NAME Configuration-Qt${QT_MAJOR_VERSION} COMMAND ConfigurationTest${QT_MAJOR_VERSION})

target_link_libraries(ConfigurationTest${QT_MAJOR_VERSION} Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)

set(QMLThemeConfigTest_SRCS QMLThemeConfigTest.cpp ../src/common/ThemeConfig.cpp ../src/common/ThemeConfig.h)
add_executable(QMLThemeConfigTest${QT_MAJOR_VERSION} ${QMLThemeConfigTest_SRCS})
target_include_directories(QMLThemeConfigTest${QT_MAJOR_VERSION} PRIVATE ../src/common/)
add_test(NAME QMLThemeConfig-Qt${QT_MAJOR_VERSION} COMMAND QMLThemeConfigTest${QT_MAJOR_VERSION} -platform offscreen -input ${CMAKE_CURRENT_SOURCE_DIR}/QMLThemeConfigTest.qml WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QMLThemeConfigTest${QT_MAJOR_VERSION} PRIVATE Qt${QT_MAJOR_VERSION}::Quick Qt${QT_MAJOR_VERSION}::QuickTest)

set(SessionTest_SRCS SessionTest.cpp ../src/common/Configuration.cpp ../src/common/ConfigReader.cpp ../src/common/Session.cpp)
add_executable(SessionTest${QT_MAJOR_VERSION} ${SessionTest_SRCS})
target_include_directories(SessionTest${QT_MAJOR_VERSION} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../src/common)
add_test(NAME Session-Qt${QT_MAJOR_VERSION} COMMAND SessionTest${QT_MAJOR_VERSION})
target_link_libraries(SessionTest${QT_MAJOR_VERSION} Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
