#=============================================================================
# Copyright (C) 2013-2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Pier Luigi Fiorini nor the names of his
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_program(QTWAYLAND_SCANNER_EXECUTABLE NAMES qtwaylandscanner)

# qtwayland_add_protocol_client(outfiles inputfile basename [interfaceprefix])
function(QTWAYLAND_ADD_PROTOCOL_CLIENT _sources _protocol _basename)
    if(QTWAYLAND_SCANNER_EXECUTABLE EQUAL "QTWAYLAND_SCANNER_EXECUTABLE-NOTFOUND")
        message(FATAL "The qtwayland-scanner executable has not been found on your system. Make sure QTDIR/bin is in your PATH.")
    endif()

    set(_extra_args ${ARGN})
    list(LENGTH _extra_args _num_extra_args)
    if(${_num_extra_args} GREATER 0)
        list(GET _extra_args 0 _prefix)
    else()
        set(_prefix "")
    endif()

    get_filename_component(_infile ${_protocol} ABSOLUTE)
    set(_cheader "${CMAKE_CURRENT_BINARY_DIR}/wayland-${_basename}-client-protocol.h")
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/qwayland-${_basename}.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/qwayland-${_basename}.cpp")

    add_custom_command(OUTPUT "${_header}"
        COMMAND ${QTWAYLAND_SCANNER_EXECUTABLE} client-header ${_infile} "" ${_prefix} > ${_header}
        DEPENDS ${_infile} ${_cheader} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${QTWAYLAND_SCANNER_EXECUTABLE} client-code ${_infile} "" ${_prefix} > ${_code}
        DEPENDS ${_infile} ${_header} VERBATIM)

    list(APPEND ${_sources} "${_code}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()

# qtwayland_add_protocol_server(outfiles inputfile basename [interfaceprefix])
function(QTWAYLAND_ADD_PROTOCOL_SERVER _sources _protocol _basename)
    if(QTWAYLAND_SCANNER_EXECUTABLE EQUAL "QTWAYLAND_SCANNER_EXECUTABLE-NOTFOUND")
        message(FATAL "The qtwayland-scanner executable has not been found on your system. Make sure QTDIR/bin is in your PATH.")
    endif()

    set(_extra_args ${ARGN})
    list(LENGTH _extra_args _num_extra_args)
    if(${_num_extra_args} GREATER 0)
        list(GET _extra_args 0 _prefix)
    else()
        set(_prefix "")
    endif()

    get_filename_component(_infile ${_protocol} ABSOLUTE)
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${_basename}.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${_basename}.cpp")

    add_custom_command(OUTPUT "${_header}"
        COMMAND ${QTWAYLAND_SCANNER_EXECUTABLE} server-header ${_infile} "" ${_prefix} > ${_header}
        DEPENDS ${_infile} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${QTWAYLAND_SCANNER_EXECUTABLE} server-code ${_infile} "" ${_prefix} > ${_code}
        DEPENDS ${_infile} ${_header} VERBATIM)

    list(APPEND ${_sources} "${_code}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()
