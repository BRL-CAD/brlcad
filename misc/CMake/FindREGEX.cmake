#.rst:
# FindREGEX
# --------
#
# Find the native REGEX includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``REGEX::REGEX``, if
# REGEX has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   REGEX_INCLUDE_DIRS   - where to find regex.h, etc.
#   REGEX_LIBRARIES      - List of libraries when using regex.
#   REGEX_FOUND          - True if regex found.
#
#
# Hints
# ^^^^^
#
# A user may set ``REGEX_ROOT`` to a regex installation root to tell this
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

set(_REGEX_SEARCHES)

# Search REGEX_ROOT first if it is set.
if(REGEX_ROOT)
  set(_REGEX_SEARCH_ROOT PATHS ${REGEX_ROOT} NO_DEFAULT_PATH)
  list(APPEND _REGEX_SEARCHES _REGEX_SEARCH_ROOT)
endif()

# Normal search.
set(_REGEX_SEARCH_NORMAL
	PATHS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Regex;InstallPath]"
        "$ENV{PROGRAMFILES}/regex"
  )
list(APPEND _REGEX_SEARCHES _REGEX_SEARCH_NORMAL)

set(REGEX_NAMES regex_brl regex_brld regex regexd regexd1)

# Try each search configuration.
foreach(search ${_REGEX_SEARCHES})
  find_path(REGEX_INCLUDE_DIR NAMES regex.h        ${${search}} PATH_SUFFIXES include)
  find_library(REGEX_LIBRARY  NAMES ${REGEX_NAMES} ${${search}} PATH_SUFFIXES lib)
endforeach()

mark_as_advanced(REGEX_LIBRARY REGEX_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set REGEX_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(REGEX REQUIRED_VARS REGEX_LIBRARY REGEX_INCLUDE_DIR)

if(REGEX_FOUND)
    set(REGEX_INCLUDE_DIRS ${REGEX_INCLUDE_DIR})
    set(REGEX_LIBRARIES ${REGEX_LIBRARY})

    if(NOT TARGET REGEX::REGEX)
      add_library(REGEX::REGEX UNKNOWN IMPORTED)
      set_target_properties(REGEX::REGEX PROPERTIES
        IMPORTED_LOCATION "${REGEX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${REGEX_INCLUDE_DIRS}")
    endif()
endif()
