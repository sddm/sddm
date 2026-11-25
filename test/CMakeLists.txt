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

include_directories(../src/auth)
add_executable(AuthMessagesTest AuthMessagesTest.cpp)
add_test(NAME AuthMessages COMMAND AuthMessagesTest)
target_link_libraries(AuthMessagesTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Qml Qt${QT_MAJOR_VERSION}::Test)

add_executable(AuthAttemptGateTest AuthAttemptGateTest.cpp)
target_include_directories(AuthAttemptGateTest PRIVATE ../src/daemon)
target_link_libraries(AuthAttemptGateTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME AuthAttemptGate COMMAND AuthAttemptGateTest)

# Compile the real Auth transport against a private fake helper executable.
# This path is local to the test binary; production sddm keeps its configured libexec path.
set(AUTH_TEST_HELPER_DIR "${CMAKE_CURRENT_BINARY_DIR}/transport-helper")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/auth-test-constants")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/auth-test-constants/Constants.h"
    CONTENT "#define LIBEXEC_INSTALL_DIR \"${AUTH_TEST_HELPER_DIR}\"\n")
add_executable(AuthTransportHelper AuthTransportHelper.cpp)
set_target_properties(AuthTransportHelper PROPERTIES
    OUTPUT_NAME sddm-helper RUNTIME_OUTPUT_DIRECTORY "${AUTH_TEST_HELPER_DIR}")
target_link_libraries(AuthTransportHelper Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Qml)

add_executable(AuthTransportTest AuthTransportTest.cpp
    ../src/auth/Auth.cpp ../src/auth/AuthRequest.cpp ../src/auth/AuthPrompt.cpp
    ../src/common/SafeDataStream.cpp)
target_include_directories(AuthTransportTest BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/auth-test-constants")
add_dependencies(AuthTransportTest AuthTransportHelper)
target_link_libraries(AuthTransportTest
    Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Qml Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME AuthTransport COMMAND AuthTransportTest)

# Exercise the real correlated proxy against a private daemon peer.
set(GreeterTest_SRCS
    ../src/greeter/GreeterProxy.cpp ../src/greeter/SessionModel.cpp
    ../src/common/Session.cpp ../src/common/Configuration.cpp
    ../src/common/ConfigReader.cpp ../src/common/SocketWriter.cpp)
add_executable(GreeterProxyTest GreeterProxyTest.cpp ${GreeterTest_SRCS})
target_include_directories(GreeterProxyTest PRIVATE
    ../src/greeter ${CMAKE_CURRENT_BINARY_DIR}/../src/common)
target_link_libraries(GreeterProxyTest Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME GreeterProxy COMMAND GreeterProxyTest)

# Compile the actual daemon lifecycle with a private helper and no host VT.
set(DAEMON_CONTEXT_HELPER_DIR "${CMAKE_CURRENT_BINARY_DIR}/daemon-context-helper")
file(READ "${CMAKE_CURRENT_BINARY_DIR}/../src/common/Constants.h" DAEMON_TEST_CONSTANTS)
string(REGEX REPLACE "#define LIBEXEC_INSTALL_DIR [^\n]*"
    "#define LIBEXEC_INSTALL_DIR \"${DAEMON_CONTEXT_HELPER_DIR}\""
    DAEMON_TEST_CONSTANTS "${DAEMON_TEST_CONSTANTS}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/daemon-test-constants")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/daemon-test-constants/Constants.h" "${DAEMON_TEST_CONSTANTS}")
add_executable(DaemonContextHelper DaemonContextHelper.cpp)
set_target_properties(DaemonContextHelper PROPERTIES
    OUTPUT_NAME sddm-helper RUNTIME_OUTPUT_DIRECTORY "${DAEMON_CONTEXT_HELPER_DIR}")
target_link_libraries(DaemonContextHelper Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Qml)


add_executable(FingerprintAvailabilityService FingerprintAvailabilityService.cpp)
target_link_libraries(FingerprintAvailabilityService Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::DBus)
get_target_property(DaemonTest_SRCS sddm SOURCES)
set(DaemonTest_ABSOLUTE_SRCS)
foreach(source IN LISTS DaemonTest_SRCS)
    if(IS_ABSOLUTE "${source}")
        list(APPEND DaemonTest_ABSOLUTE_SRCS "${source}")
        if("${source}" MATCHES "^${CMAKE_BINARY_DIR}/")
            set_source_files_properties("${source}" PROPERTIES GENERATED TRUE SKIP_AUTOMOC ON)
        endif()
    else()
        list(APPEND DaemonTest_ABSOLUTE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../src/daemon/${source}")
    endif()
endforeach()
add_executable(DaemonContextTest DaemonContextTest.cpp ${DaemonTest_ABSOLUTE_SRCS}
    ../src/greeter/GreeterProxy.cpp ../src/greeter/SessionModel.cpp)
target_compile_definitions(DaemonContextTest PRIVATE SDDM_DAEMON_APP_TEST)
target_include_directories(DaemonContextTest BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/daemon-test-constants"
    ../src/daemon ../src/greeter ${CMAKE_CURRENT_BINARY_DIR}/../src/daemon
    ${CMAKE_CURRENT_BINARY_DIR}/../src/common ${LIBXAU_INCLUDE_DIRS} ${LIBXCB_INCLUDE_DIR})
get_target_property(DaemonTest_LIBS sddm LINK_LIBRARIES)
target_link_libraries(DaemonContextTest ${DaemonTest_LIBS} Qt${QT_MAJOR_VERSION}::Test)
add_dependencies(DaemonContextTest DaemonContextHelper FingerprintAvailabilityService sddm)
add_test(NAME DaemonContext COMMAND DaemonContextTest)

if(QT_MAJOR_VERSION EQUAL "5")
    set(GREETER_TEST_TARGET sddm-greeter)
else()
    set(GREETER_TEST_TARGET sddm-greeter-qt${QT_MAJOR_VERSION})
endif()
get_target_property(GreeterAppTest_SRCS ${GREETER_TEST_TARGET} SOURCES)
set(GreeterAppTest_ABSOLUTE_SRCS)
foreach(source IN LISTS GreeterAppTest_SRCS)
    if(IS_ABSOLUTE "${source}")
        list(APPEND GreeterAppTest_ABSOLUTE_SRCS "${source}")
        if("${source}" MATCHES "^${CMAKE_BINARY_DIR}/")
            set_source_files_properties("${source}" PROPERTIES GENERATED TRUE SKIP_AUTOMOC ON)
        endif()
    else()
        list(APPEND GreeterAppTest_ABSOLUTE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../src/greeter/${source}")
    endif()
endforeach()
add_executable(GreeterAppTest GreeterAppTest.cpp ${GreeterAppTest_ABSOLUTE_SRCS})
target_compile_definitions(GreeterAppTest PRIVATE SDDM_GREETER_APP_TEST)
get_target_property(GreeterAppTest_INCLUDES ${GREETER_TEST_TARGET} INCLUDE_DIRECTORIES)
target_include_directories(GreeterAppTest PRIVATE ${GreeterAppTest_INCLUDES} ../src/greeter)
get_target_property(GreeterAppTest_LIBS ${GREETER_TEST_TARGET} LINK_LIBRARIES)
target_link_libraries(GreeterAppTest ${GreeterAppTest_LIBS} Qt${QT_MAJOR_VERSION}::Test)
add_dependencies(GreeterAppTest ${GREETER_TEST_TARGET})
add_test(NAME GreeterApp COMMAND GreeterAppTest -platform offscreen)
set_tests_properties(GreeterApp PROPERTIES
    ENVIRONMENT "QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic")
