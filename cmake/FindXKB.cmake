# - Try to find libxcb
# Once done this will define
#
#  LIBXKB_FOUND - system has libxcb
#  LIBXKB_LIBRARIES - Link these to use libxcb-xkb
#  LIBXKB_INCLUDE_DIR - the libxcb-xkb include dir
#  LIBXKB_DEFINITIONS - compiler switches required for using libxcb

# Copyright (c) 2013, Abdurrahman AVCI, <abdurrahmanavci@gmail.com>
# Copyright (c) 2008, Helio Chissini de Castro, <helio@kde.org>
# Copyright (c) 2007, Matthias Kretz, <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


IF (NOT WIN32)
  IF (LIBXKB_INCLUDE_DIR AND LIBXKB_LIBRARIES)
    # in cache already
    SET(XKB_FIND_QUIETLY TRUE)
  ENDIF (LIBXKB_INCLUDE_DIR AND LIBXKB_LIBRARIES)

  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  FIND_PACKAGE(PkgConfig)
  PKG_CHECK_MODULES(PKG_XKB xcb-xkb)

  SET(LIBXKB_DEFINITIONS ${PKG_XKB_CFLAGS})

  FIND_PATH(LIBXKB_INCLUDE_DIR xcb/xkb.h ${PKG_XKB_INCLUDE_DIRS})

  FIND_LIBRARY(LIBXKB_LIBRARIES NAMES xcb-xkb libxcb-xkb PATHS ${PKG_XKB_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XKB DEFAULT_MSG LIBXKB_LIBRARIES LIBXKB_INCLUDE_DIR)

  MARK_AS_ADVANCED(LIBXKB_INCLUDE_DIR LIBXKB_LIBRARIES)
ENDIF (NOT WIN32)
