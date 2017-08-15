# - Try to find libXau
# Once done this will define
#
#  LIBXAU_FOUND - system has libXau
#  LIBXAU_LIBRARIES - Link these to use libXau
#  LIBXAU_INCLUDE_DIR - the libXau include dir
#  LIBXAU_DEFINITIONS - compiler switches required for using libXau

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
  IF (LIBXAU_INCLUDE_DIR AND LIBXAU_LIBRARIES)
    # in cache already
    SET(XAU_FIND_QUIETLY TRUE)
  ENDIF (LIBXAU_INCLUDE_DIR AND LIBXAU_LIBRARIES)

  FIND_PACKAGE(PkgConfig)
  PKG_CHECK_MODULES(PKG_XAU xau)

  SET(LIBXAU_DEFINITIONS ${PKG_XAU_CFLAGS})

  FIND_PATH(LIBXAU_INCLUDE_DIR XAuth.h ${PKG_XAU_INCLUDE_DIR})
  FIND_LIBRARY(LIBXAU_LIBRARIES NAMES Xau libXau PATHS ${PKG_XAU_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(XAU DEFAULT_MSG LIBXAU_LIBRARIES)

  MARK_AS_ADVANCED(LIBXAU_INCLUDE_DIR LIBXAU_LIBRARIES)
ENDIF (NOT WIN32)
