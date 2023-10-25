#.rst:
# FindLINENOISE
# --------
#
# Find the native LINENOISE includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``LINENOISE::LINENOISE``, if
# LINENOISE has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   LINENOISE_INCLUDE_DIRS   - where to find linenoise.h, etc.
#   LINENOISE_LIBRARIES      - List of libraries when using linenoise.
#   LINENOISE_FOUND          - True if linenoise found.
#
# Hints
# ^^^^^
#
# A user may set ``LINENOISE_ROOT`` to a linenoise installation root to tell this
# module where to look.

#=============================================================================
# Copyright 2001-2011 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

set(_LINENOISE_SEARCHES)

# Search LINENOISE_ROOT first if it is set.
if(LINENOISE_ROOT)
  set(_LINENOISE_SEARCH_ROOT PATHS ${LINENOISE_ROOT} NO_DEFAULT_PATH)
  list(APPEND _LINENOISE_SEARCHES _LINENOISE_SEARCH_ROOT)
endif()

set(LINENOISE_NAMES linenoise linenoised)

# Try each search configuration.
foreach(search ${_LINENOISE_SEARCHES})
  find_path(LINENOISE_INCLUDE_DIR NAMES linenoise.h        ${${search}} PATH_SUFFIXES include include/linenoise)
  find_library(LINENOISE_LIBRARY  NAMES ${LINENOISE_NAMES} ${${search}} PATH_SUFFIXES lib)
endforeach()

mark_as_advanced(LINENOISE_LIBRARY LINENOISE_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set LINENOISE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LINENOISE REQUIRED_VARS LINENOISE_LIBRARY LINENOISE_INCLUDE_DIR)

if(LINENOISE_FOUND)
    set(LINENOISE_INCLUDE_DIRS ${LINENOISE_INCLUDE_DIR})
    set(LINENOISE_LIBRARIES ${LINENOISE_LIBRARY})

    if(NOT TARGET LINENOISE::LINENOISE)
      add_library(LINENOISE::LINENOISE UNKNOWN IMPORTED)
      set_target_properties(LINENOISE::LINENOISE PROPERTIES
        IMPORTED_LOCATION "${LINENOISE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LINENOISE_INCLUDE_DIRS}")
    endif()
endif()
