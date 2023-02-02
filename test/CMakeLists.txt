set(QT_USE_QTTEST TRUE)

include_directories(../src/common)

set(ConfigurationTest_SRCS ConfigurationTest.cpp ../src/common/ConfigReader.cpp)
add_executable(ConfigurationTest ${ConfigurationTest_SRCS})
add_test(NAME Configuration COMMAND ConfigurationTest)

target_link_libraries(ConfigurationTest Qt5::Core Qt5::Test)

set(QMLThemeConfigTest_SRCS QMLThemeConfigTest.cpp ../src/common/ThemeConfig.cpp ../src/common/ThemeConfig.h)
add_executable(QMLThemeConfigTest ${QMLThemeConfigTest_SRCS})
add_test(NAME QMLThemeConfig COMMAND QMLThemeConfigTest)
target_link_libraries(QMLThemeConfigTest Qt5::QuickTest)
