# - Try to find libxcb
# Once done this will define
#
#  LIBXCB_FOUND - system has libxcb
#  LIBXCB_LIBRARIES - Link these to use libxcb
#  LIBXCB_INCLUDE_DIR - the libxcb include dir
#  LIBXCB_DEFINITIONS - compiler switches required for using libxcb

# Copyright (c) 2008 Helio Chissini de Castro, <helio@kde.org>
# Copyright (c) 2007, Matthias Kretz, <kretz@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


IF (NOT WIN32)
  IF (LIBXCB_INCLUDE_DIR AND LIBXCB_LIBRARIES)
    # in cache already
    SET(XCB_FIND_QUIETLY TRUE)
  ENDIF (LIBXCB_INCLUDE_DIR AND LIBXCB_LIBRARIES)

  FIND_PACKAGE(PkgConfig)
  PKG_CHECK_MODULES(PKG_XCB xcb)

  SET(LIBXCB_DEFINITIONS ${PKG_XCB_CFLAGS})

  FIND_PATH(LIBXCB_INCLUDE_DIR xcb/xcb.h ${PKG_XCB_INCLUDE_DIRS})
  FIND_LIBRARY(LIBXCB_LIBRARIES NAMES xcb libxcb PATHS ${PKG_XCB_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XCB DEFAULT_MSG LIBXCB_LIBRARIES LIBXCB_INCLUDE_DIR)

  MARK_AS_ADVANCED(LIBXCB_INCLUDE_DIR LIBXCB_LIBRARIES XCBPROC_EXECUTABLE)
ENDIF (NOT WIN32)
