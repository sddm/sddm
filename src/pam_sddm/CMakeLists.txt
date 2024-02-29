include(KDEInstallDirs)

find_package(PAM REQUIRED)
find_package(keyutils REQUIRED)

add_definitions(-DHAVE_KEYUTILS)

include_directories(
    ${PAM_INCLUDE_DIR}
    ${KEYUTILS_INCLUDE_DIR}
)

add_library(pam_sddm SHARED pam_sddm.c)
set_target_properties(pam_sddm PROPERTIES PREFIX "")
target_link_libraries(pam_sddm
    ${PAM_LIBRARIES}
    ${KEYUTILS_LIBRARIES}
)

install(TARGETS pam_sddm DESTINATION ${KDE_INSTALL_LIBDIR}/security)
