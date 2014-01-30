# - Try to find the QAuth libraries
# Once done this will define
#
#  QAUTH_FOUND - system has QAuth
#  QAUTH_INCLUDE_DIR - the QAuth include directory
#  QAUTH_LIBRARIES - libpam library

if (QAUTH_LIBRARIES)
	# Already in cache, be silent
	set(QAUTH_FIND_QUIETLY TRUE)
endif (QAUTH_LIBRARIES)

find_library(QAUTH_LIBRARIES qauth)

if (QAUTH_LIBRARIES)
	set(QAUTH_FOUND TRUE)
endif (QAUTH_LIBRARIES)

if (QAUTH_FOUND)
	if (NOT QAUTH_FIND_QUIETLY)
		message(STATUS "Found QAuth: ${QAUTH_LIBRARIES}")
	endif (NOT QAUTH_FIND_QUIETLY)
else (QAUTH_FOUND)
	if (QAUTH_FIND_REQUIRED)
		message(FATAL_ERROR "QAuth was not found")
	endif(QAUTH_FIND_REQUIRED)
endif (QAUTH_FOUND)

mark_as_advanced(QAUTH_INCLUDE_DIR QAUTH_LIBRARIES)
