find_package(PythonInterp 3 REQUIRED)
set(SDDM_SOURCE ${CMAKE_SOURCE_DIR})
set(SDDM_COMMON ${CMAKE_BINARY_DIR}/src/common)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/prepare.py
  ${SDDM_SOURCE}/src/daemon/SocketServer.cpp
  ${SDDM_SOURCE}/src/daemon/Display.cpp
  ${SDDM_SOURCE}/src/helper/HelperApp.cpp
  ${SDDM_SOURCE}/src/helper/backend/PamBackend.cpp
  ${SDDM_SOURCE}/src/auth/Auth.cpp
  ${SDDM_SOURCE}/src/greeter/theme/Main.qml)
execute_process(COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prepare.py
  ${SDDM_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/generated RESULT_VARIABLE fixture_status)
if(NOT fixture_status EQUAL 0)
  message(FATAL_ERROR "Could not prepare production-method authentication fixtures")
endif()
add_executable(ConversationTest proofs.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/SocketServer-fixture.cpp
  ${SDDM_SOURCE}/src/daemon/SocketServer.h
  ${SDDM_SOURCE}/src/greeter/GreeterProxy.cpp ${SDDM_SOURCE}/src/greeter/GreeterProxy.h
  ${SDDM_SOURCE}/src/common/SocketWriter.cpp ${SDDM_SOURCE}/src/common/Session.cpp
  ${SDDM_SOURCE}/src/common/Configuration.cpp ${SDDM_SOURCE}/src/common/ConfigReader.cpp)
target_include_directories(ConversationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated ${SDDM_SOURCE}/src/common ${SDDM_SOURCE}/src/greeter
  ${SDDM_SOURCE}/src/daemon ${SDDM_SOURCE}/src/auth ${SDDM_COMMON})
target_link_libraries(ConversationTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network
  Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME Conversation COMMAND ConversationTest)

add_executable(AuthLifecycleTest auth-lifecycle.cpp
  ${SDDM_SOURCE}/src/auth/Auth.cpp ${SDDM_SOURCE}/src/auth/Auth.h
  ${SDDM_SOURCE}/src/auth/AuthRequest.cpp ${SDDM_SOURCE}/src/auth/AuthRequest.h
  ${SDDM_SOURCE}/src/auth/AuthPrompt.cpp ${SDDM_SOURCE}/src/auth/AuthPrompt.h
  ${SDDM_SOURCE}/src/common/SafeDataStream.cpp)
target_include_directories(AuthLifecycleTest PRIVATE ${SDDM_SOURCE}/src/auth
  ${SDDM_SOURCE}/src/common ${SDDM_COMMON})
target_link_libraries(AuthLifecycleTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network
  Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Qml)
target_compile_definitions(AuthLifecycleTest PRIVATE CONVERSATION_PYTHON="${PYTHON_EXECUTABLE}")
add_test(NAME AuthLifecycle COMMAND AuthLifecycleTest)

add_executable(AuthFrameTest frame-test.cpp ${SDDM_SOURCE}/src/common/SafeDataStream.cpp)
target_include_directories(AuthFrameTest PRIVATE ${SDDM_SOURCE}/src/common ${SDDM_SOURCE}/src/auth ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(AuthFrameTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME AuthFrame COMMAND AuthFrameTest)

add_executable(ConversationThemeTest theme-test.cpp)
target_include_directories(ConversationThemeTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(ConversationThemeTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME ConversationTheme COMMAND ConversationThemeTest)

add_executable(AuthHandshakeTest handshake-test.cpp ${SDDM_SOURCE}/src/common/SafeDataStream.cpp)
target_include_directories(AuthHandshakeTest PRIVATE ${SDDM_SOURCE}/src/common ${SDDM_SOURCE}/src/auth ${SDDM_COMMON} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(AuthHandshakeTest Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME AuthHandshake COMMAND AuthHandshakeTest)
