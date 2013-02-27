set(SOURCES
  Application.cpp
  Authenticator.cpp
  Configuration.cpp
  Cookie.cpp
  DisplayManager.cpp
  LockFile.cpp
  Main.cpp
  PowerManager.cpp
  ScreenModel.cpp
  SessionManager.cpp
  SessionModel.cpp
  ThemeConfig.cpp
  ThemeMetadata.cpp
  UserModel.cpp
  Util.cpp
)

if(USE_QT5)
  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB})

  qt5_use_modules(sddm Quick DBus)
else()
  set(QT_USE_QTNETWORK TRUE)
  set(QT_USE_QTDECLARATIVE TRUE)
  set(QT_USE_QTDBUS TRUE)
  include(${QT_USE_FILE})

  add_executable(sddm ${SOURCES})
  target_link_libraries(sddm ${PAM_LIBRARIES} ${X11_X11_LIB} ${QT_LIBRARIES})
endif(USE_QT5)

install(TARGETS sddm DESTINATION ${BIN_INSTALL_DIR})
