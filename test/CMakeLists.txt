set(QT_USE_QTTEST TRUE)

include_directories(../src/common)


set(ConfigurationTest_SRCS ConfigurationTest.cpp ../src/common/ConfigReader.cpp)
add_executable(ConfigurationTest ${ConfigurationTest_SRCS})
add_test(NAME Configuration COMMAND ConfigurationTest)

if(USE_QT5)
    qt5_use_modules(ConfigurationTest Test)
else()
    include(${QT_USE_FILE})
    target_link_libraries(ConfigurationTest ${QT_LIBRARIES})
endif()