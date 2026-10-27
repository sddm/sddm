cmake_minimum_required(VERSION 3.20)
project(ParallelAuthTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(PAM_FINGERPRINT_SERVICE "sddm-fingerprint" CACHE STRING "Fingerprint PAM service under test")
find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Test DBus)
get_filename_component(SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SDDM_NATIVE_BUILD "" CACHE PATH "Completed native SDDM build with helper and generated daemon interfaces")
if(NOT EXISTS "${SDDM_NATIVE_BUILD}/src/daemon/Login1Manager.cpp")
 message(FATAL_ERROR "Set SDDM_NATIVE_BUILD to a build directory with generated daemon interfaces")
endif()
configure_file(Constants.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/Constants.h @ONLY)
set(AUTH_SOURCES ${SOURCE}/src/auth/Auth.cpp ${SOURCE}/src/auth/AuthPrompt.cpp ${SOURCE}/src/auth/AuthRequest.cpp ${SOURCE}/src/common/SafeDataStream.cpp)
add_executable(FakeAuthHelper FakeAuthHelper.cpp ${SOURCE}/src/common/SafeDataStream.cpp)
set_target_properties(FakeAuthHelper PROPERTIES OUTPUT_NAME sddm-helper RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fake-bin)
add_executable(ParallelAuthTest ParallelAuthTest.cpp ${AUTH_SOURCES})
set(TEST_EMBEDDED_HELPER "${CMAKE_CURRENT_BINARY_DIR}/embedded-test-helper.cpp")
add_custom_command(OUTPUT "${TEST_EMBEDDED_HELPER}"
 COMMAND ${CMAKE_COMMAND} -DHELPER=$<TARGET_FILE:FakeAuthHelper> -DOUTPUT=${TEST_EMBEDDED_HELPER}
 -P ${SOURCE}/cmake/EmbedHelper.cmake DEPENDS FakeAuthHelper ${SOURCE}/cmake/EmbedHelper.cmake VERBATIM)
set_source_files_properties("${TEST_EMBEDDED_HELPER}" PROPERTIES SKIP_AUTOMOC ON)
target_sources(ParallelAuthTest PRIVATE "${TEST_EMBEDDED_HELPER}")
target_compile_definitions(ParallelAuthTest PRIVATE SDDM_EMBEDDED_PARALLEL_HELPER=1 SDDM_PARALLEL_TEST=1)
add_executable(RealHelperTest RealHelperTest.cpp ${SOURCE}/src/common/SafeDataStream.cpp)
add_executable(SocketPermissionTest SocketPermissionTest.cpp ${AUTH_SOURCES})
add_executable(NoPeerAuthTest ParallelAuthTest.cpp ${AUTH_SOURCES} ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp)
add_executable(NoSyscallAuthTest ParallelAuthTest.cpp ${AUTH_SOURCES} ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp)
target_compile_definitions(NoSyscallAuthTest PRIVATE SDDM_TEST_MISSING_SYSCALLS=1)
target_compile_definitions(NoPeerAuthTest PRIVATE SDDM_TEST_NO_PEER_PID=1)
foreach(target FakeAuthHelper ParallelAuthTest RealHelperTest NoPeerAuthTest NoSyscallAuthTest SocketPermissionTest)
 target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/auth ${SOURCE}/src/common)
 target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Network Qt6::Qml Qt6::Test)
 target_compile_options(${target} PRIVATE -Wall -Wextra)
endforeach()
add_dependencies(ParallelAuthTest FakeAuthHelper)
add_dependencies(NoPeerAuthTest FakeAuthHelper)
add_dependencies(NoSyscallAuthTest FakeAuthHelper)
enable_testing()
add_test(NAME ParallelAuth COMMAND ParallelAuthTest)
add_test(NAME SocketPermission COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/socket-permission-test.sh $<TARGET_FILE:SocketPermissionTest>)
add_test(NAME NoPeerAuth COMMAND NoPeerAuthTest legacyFramingWorksWhenParallelDisabled unavailablePeerCredentialsDisableParallel cancelledPromptCanCompleteWithoutSocket)
add_test(NAME NoSyscallAuth COMMAND NoSyscallAuthTest legacyFramingWorksWhenParallelDisabled unavailablePeerCredentialsDisableParallel cancelledPromptCanCompleteWithoutSocket)
# Compile the helper from this source tree; the external native build only
# supplies generated daemon interfaces used by DisplayLifecycleTest.
set(HELPER_ROOT ${SOURCE}/src/helper)
add_executable(V5RealHelper ${AUTH_SOURCES}
 ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp
 ${SOURCE}/src/common/XAuth.cpp ${SOURCE}/src/common/SignalHandler.cpp
 ${SOURCE}/src/common/VirtualTerminal.cpp ${HELPER_ROOT}/Backend.cpp
 ${HELPER_ROOT}/HelperApp.cpp ${HELPER_ROOT}/UserSession.cpp
 ${HELPER_ROOT}/backend/PamHandle.cpp ${HELPER_ROOT}/backend/PamBackend.cpp)
target_include_directories(V5RealHelper PRIVATE ${HELPER_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/common ${SOURCE}/src/auth)
target_link_libraries(V5RealHelper PRIVATE Qt6::Core Qt6::Network Qt6::DBus Qt6::Qml Xau pam)
add_test(NAME RealHelper COMMAND RealHelperTest $<TARGET_FILE:V5RealHelper>)
set(PAIR_EMBEDDED_HELPER "${CMAKE_CURRENT_BINARY_DIR}/embedded-pair-helper.cpp")
add_custom_command(OUTPUT "${PAIR_EMBEDDED_HELPER}"
 COMMAND ${CMAKE_COMMAND} -DHELPER=$<TARGET_FILE:V5RealHelper> -DOUTPUT=${PAIR_EMBEDDED_HELPER}
 -P ${SOURCE}/cmake/EmbedHelper.cmake DEPENDS V5RealHelper ${SOURCE}/cmake/EmbedHelper.cmake VERBATIM)
set_source_files_properties("${PAIR_EMBEDDED_HELPER}" PROPERTIES SKIP_AUTOMOC ON)
add_executable(ActualPairTest ActualPairTest.cpp ${AUTH_SOURCES} "${PAIR_EMBEDDED_HELPER}")
target_compile_definitions(ActualPairTest PRIVATE SDDM_EMBEDDED_PARALLEL_HELPER=1 SDDM_PARALLEL_TEST=1 SDDM_TEST_HELPER_DIR="${CMAKE_CURRENT_BINARY_DIR}/pair-bin")
target_include_directories(ActualPairTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/common ${SOURCE}/src/auth)
target_link_libraries(ActualPairTest PRIVATE Qt6::Core Qt6::Network Qt6::Qml Qt6::Test)
add_executable(LoginMessagesTest LoginMessagesTest.cpp ${SOURCE}/src/common/SocketWriter.cpp ${SOURCE}/src/common/Session.cpp ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp)
target_include_directories(LoginMessagesTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/common)
target_link_libraries(LoginMessagesTest PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME LoginMessages COMMAND LoginMessagesTest)

add_executable(GreeterHandshakeTest GreeterHandshakeTest.cpp ${SOURCE}/src/greeter/GreeterProxy.cpp ${SOURCE}/src/greeter/SessionModel.cpp ${SOURCE}/src/common/SocketWriter.cpp ${SOURCE}/src/common/Session.cpp ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp)
target_include_directories(GreeterHandshakeTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/common ${SOURCE}/src/greeter)
target_link_libraries(GreeterHandshakeTest PRIVATE Qt6::Core Qt6::Network Qt6::Test)
add_test(NAME GreeterHandshake COMMAND GreeterHandshakeTest)

find_package(Qt6 REQUIRED COMPONENTS DBus)
file(GLOB DISPLAY_DAEMON_SOURCES ${SOURCE}/src/daemon/*.cpp)
list(FILTER DISPLAY_DAEMON_SOURCES EXCLUDE REGEX "/(DaemonApp|Seat|Greeter)\\.cpp$")
file(GLOB DISPLAY_DBUS_SOURCES ${SDDM_NATIVE_BUILD}/src/daemon/*adaptor.cpp ${SDDM_NATIVE_BUILD}/src/daemon/Login1*.cpp)
list(FILTER DISPLAY_DBUS_SOURCES EXCLUDE REGEX "/moc_")
add_executable(DisplayLifecycleTest DisplayLifecycleTest.cpp DisplayLeaves.cpp ${DISPLAY_DAEMON_SOURCES} ${DISPLAY_DBUS_SOURCES}
 ${SOURCE}/src/daemon/DaemonApp.h ${SOURCE}/src/daemon/Seat.h ${SOURCE}/src/daemon/Greeter.h
 ${AUTH_SOURCES} ${SOURCE}/src/common/Configuration.cpp ${SOURCE}/src/common/ConfigReader.cpp ${SOURCE}/src/common/Session.cpp ${SOURCE}/src/common/SocketWriter.cpp
 ${SOURCE}/src/common/ThemeConfig.cpp ${SOURCE}/src/common/ThemeMetadata.cpp ${SOURCE}/src/common/XAuth.cpp ${SOURCE}/src/common/SignalHandler.cpp ${SOURCE}/src/common/VirtualTerminal.cpp)
target_sources(DisplayLifecycleTest PRIVATE "${TEST_EMBEDDED_HELPER}")
target_compile_definitions(DisplayLifecycleTest PRIVATE SDDM_EMBEDDED_PARALLEL_HELPER=1 SDDM_PARALLEL_TEST=1)
target_include_directories(DisplayLifecycleTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/common ${SOURCE}/src/auth ${SOURCE}/src/daemon ${SDDM_NATIVE_BUILD}/src/daemon)
target_link_libraries(DisplayLifecycleTest PRIVATE Qt6::Core Qt6::Network Qt6::DBus Qt6::Qml Qt6::Test Xau xcb)
add_dependencies(DisplayLifecycleTest FakeAuthHelper)
add_test(NAME DisplayLifecycle COMMAND /usr/bin/dbus-run-session -- /bin/bash ${CMAKE_SOURCE_DIR}/run-display-test.sh $<TARGET_FILE:DisplayLifecycleTest>)

set_tests_properties(DisplayLifecycle PROPERTIES TIMEOUT 90)

# Isolated real-helper test of the opt-in private PAM readiness signal.
add_executable(ReadyHelperTest ReadyHelperTest.cpp ${SOURCE}/src/common/SafeDataStream.cpp)
target_include_directories(ReadyHelperTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${SOURCE}/src/auth ${SOURCE}/src/common)
target_link_libraries(ReadyHelperTest PRIVATE Qt6::Core Qt6::Network Qt6::Qml Qt6::Test)
