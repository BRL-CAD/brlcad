#.rst:
# FindLMDB
# --------
#
# Find the native LMDB includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``LMDB::LMDB``, if
# LMDB has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   LMDB_INCLUDE_DIRS   - where to find lmdb.h, etc.
#   LMDB_LIBRARIES      - List of libraries when using lmdb.
#   LMDB_FOUND          - True if lmdb found.
#
# Hints
# ^^^^^
#
# A user may set ``LMDB_ROOT`` to a lmdb installation root to tell this
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

set(_LMDB_SEARCHES)

# Search LMDB_ROOT first if it is set.
if(LMDB_ROOT)
  set(_LMDB_SEARCH_ROOT PATHS ${LMDB_ROOT} NO_DEFAULT_PATH)
  list(APPEND _LMDB_SEARCHES _LMDB_SEARCH_ROOT)
endif()

set(LMDB_NAMES lmdb lmdbd)

# Try each search configuration.
foreach(search ${_LMDB_SEARCHES})
  find_path(LMDB_INCLUDE_DIR NAMES lmdb.h        ${${search}} PATH_SUFFIXES include include/lmdb)
  find_library(LMDB_LIBRARY  NAMES ${LMDB_NAMES} ${${search}} PATH_SUFFIXES lib)
endforeach()

mark_as_advanced(LMDB_LIBRARY LMDB_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set LMDB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LMDB REQUIRED_VARS LMDB_LIBRARY LMDB_INCLUDE_DIR)

if(LMDB_FOUND)
    set(LMDB_INCLUDE_DIRS ${LMDB_INCLUDE_DIR})
    set(LMDB_LIBRARIES ${LMDB_LIBRARY})

    if(NOT TARGET LMDB::LMDB)
      add_library(LMDB::LMDB UNKNOWN IMPORTED)
      set_target_properties(LMDB::LMDB PROPERTIES
        IMPORTED_LOCATION "${LMDB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LMDB_INCLUDE_DIRS}")
    endif()
endif()
