###############################################################################
# CMake module to search for PROJ.4 library
#
# On success, the macro sets the following variables:
# PROJ4_FOUND       = if the library found
# PROJ4_VERSION     = version number of PROJ.4 found
# PROJ4_LIBRARIES   = full path to the library
# PROJ4_INCLUDE_DIR = where to find the library headers 
# Also defined, but not for general use are
# PROJ4_LIBRARY, where to find the PROJ.4 library.
#
# Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

set(_PROJ4_SEARCHES)

# Search PROJ4_ROOT first if it is set.
if(PROJ4_ROOT)
  set(_PROJ4_SEARCH_ROOT PATHS ${PROJ4_ROOT} NO_DEFAULT_PATH)
  list(APPEND _PROJ4_SEARCHES _PROJ4_SEARCH_ROOT)
endif()

# Normal search.
set(_PROJ4_x86 "(x86)")
set(_PROJ4_SEARCH_NORMAL
    PATHS
	 "$ENV{OSGEO4W_HOME}"
	 "C:/OSGeo4W"
  	 "$ENV{ProgramFiles}/proj4"
	 "$ENV{ProgramFiles${_PROJ4_x86}}/proj4"
	 )
 unset(_PROJ4_x86)
list(APPEND _PROJ4_SEARCHES _PROJ4_SEARCH_NORMAL)

# Try each search configuration.
foreach(search ${_PROJ4_SEARCHES})
  find_path(PROJ4_INCLUDE_DIR NAMES proj_api.h ${${search}} PATH_SUFFIXES include include/proj4 proj4 include/proj proj)
endforeach()

if (PROJ4_INCLUDE_DIR)
	# Extract version from proj_api.h (ex: 480)
	file(STRINGS ${PROJ4_INCLUDE_DIR}/proj_api.h
		PJ_VERSIONSTR
  		REGEX "#define[ ]+PJ_VERSION[ ]+[0-9]+")
	string(REGEX MATCH "[0-9]+" PJ_VERSIONSTR ${PJ_VERSIONSTR})	

	# TODO: Should be formatted as 4.8.0?
	set(PROJ4_VERSION ${PJ_VERSIONSTR})
endif()

set(PROJ4_NAMES ${PROJ4_NAMES} proj proj_i)
if(NOT PROJ4_LIBRARY)
	foreach(search ${_PROJ4_SEARCHES})
		find_library(PROJ4_LIBRARY NAMES ${PROJ4_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
	endforeach()
endif()
unset(PROJ4_NAMES)

if(PROJ4_LIBRARY)
  set(PROJ4_LIBRARIES ${PROJ4_LIBRARY})
endif()

# Handle the QUIETLY and REQUIRED arguments and set PROJ4_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PROJ4 DEFAULT_MSG
	PROJ4_LIBRARY
	PROJ4_INCLUDE_DIR)
