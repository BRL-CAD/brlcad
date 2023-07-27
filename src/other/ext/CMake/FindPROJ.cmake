###############################################################################
# CMake module to search for PROJ library
#
# On success, the macro sets the following variables:
# PROJ_FOUND       = if the library found
# PROJ_VERSION     = version number of PROJ found
# PROJ_LIBRARIES   = full path to the library
# PROJ_INCLUDE_DIR = where to find the library headers
# Also defined, but not for general use are
# PROJ_LIBRARY, where to find the PROJ library.
#
# Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

set(_PROJ_SEARCHES)

# Search PROJ_ROOT first if it is set.
if(PROJ_ROOT)
  set(_PROJ_SEARCH_ROOT PATHS ${PROJ_ROOT} NO_DEFAULT_PATH)
  list(APPEND _PROJ_SEARCHES _PROJ_SEARCH_ROOT)
endif()

# Normal search.
set(_PROJ_x86 "(x86)")
set(_PROJ_SEARCH_NORMAL
  PATHS
  "$ENV{OSGEO4W_HOME}"
  "C:/OSGeo4W"
  "$ENV{ProgramFiles}/proj"
  "$ENV{ProgramFiles${_PROJ_x86}}/proj"
  )
unset(_PROJ_x86)
list(APPEND _PROJ_SEARCHES _PROJ_SEARCH_NORMAL)

# Try each search configuration.
foreach(search ${_PROJ_SEARCHES})
  find_path(PROJ_INCLUDE_DIR NAMES proj.h ${${search}} PATH_SUFFIXES include include/proj proj)
endforeach()

if (PROJ_INCLUDE_DIR)
  # Extract version from proj.h (ex: 480)
  file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h PJ_MAJORSTR REGEX "#define[ ]+PROJ_VERSION_MAJOR[ ]+[0-9]+")
  string(REGEX MATCH "[0-9]+" PJ_MAJOR ${PJ_MAJORSTR})
  file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h PJ_MINORSTR REGEX "#define[ ]+PROJ_VERSION_MINOR[ ]+[0-9]+")
  string(REGEX MATCH "[0-9]+" PJ_MINOR ${PJ_MINORSTR})
  file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h PJ_PATCHSTR REGEX "#define[ ]+PROJ_VERSION_PATCH[ ]+[0-9]+")
  string(REGEX MATCH "[0-9]+" PJ_PATCH ${PJ_PATCHSTR})
  set(PROJ_VERSION ${PJ_MAJOR}.${PJ_MINOR}.${PJ_PATCH})
endif()

set(PROJ_NAMES ${PROJ_NAMES} proj proj_i)
if(NOT PROJ_LIBRARY)
  foreach(search ${_PROJ_SEARCHES})
    find_library(PROJ_LIBRARY NAMES ${PROJ_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()
unset(PROJ_NAMES)

if(PROJ_LIBRARY)
  set(PROJ_LIBRARIES ${PROJ_LIBRARY})
endif()

# Handle the QUIETLY and REQUIRED arguments and set PROJ_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PROJ DEFAULT_MSG
  PROJ_LIBRARY
  PROJ_INCLUDE_DIR)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

