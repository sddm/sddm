set(QT_USE_QTTEST TRUE)

include_directories(../src/common)

set(ConfigurationTest_SRCS ConfigurationTest.cpp ../src/common/ConfigReader.cpp)
add_executable(ConfigurationTest ${ConfigurationTest_SRCS})
add_test(NAME Configuration COMMAND ConfigurationTest)

target_link_libraries(ConfigurationTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)

set(QMLThemeConfigTest_SRCS QMLThemeConfigTest.cpp ../src/common/ThemeConfig.cpp ../src/common/ThemeConfig.h)
add_executable(QMLThemeConfigTest ${QMLThemeConfigTest_SRCS})
target_include_directories(QMLThemeConfigTest PRIVATE ../src/common/)
add_test(NAME QMLThemeConfig COMMAND QMLThemeConfigTest -platform offscreen -input ${CMAKE_CURRENT_SOURCE_DIR}/QMLThemeConfigTest.qml WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QMLThemeConfigTest PRIVATE Qt${QT_MAJOR_VERSION}::Quick Qt${QT_MAJOR_VERSION}::QuickTest)

set(SessionTest_SRCS SessionTest.cpp ../src/common/Configuration.cpp ../src/common/ConfigReader.cpp ../src/common/Session.cpp)
add_executable(SessionTest ${SessionTest_SRCS})
target_include_directories(SessionTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../src/common)
add_test(NAME Session COMMAND SessionTest)
target_link_libraries(SessionTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
