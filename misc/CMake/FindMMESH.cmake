# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindMMESH
---------

Find the native MMESH includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``mmesh::mmesh``, if
MMESH has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  MMESH_INCLUDE_DIRS   - where to find meshdecimation.h, etc.
  MMESH_LIBRARIES      - List of libraries when using MMESH.
  MMESH_FOUND          - True if MMESH found.

Hints
^^^^^

A user may set ``MMESH_ROOT`` to an MMESH installation root to tell this
module where to look.
#]=======================================================================]

find_package(mmesh CONFIG QUIET)
if(TARGET mmesh::mmesh)
  set(MMESH_FOUND TRUE)
  get_target_property(MMESH_INCLUDE_DIRS mmesh::mmesh INTERFACE_INCLUDE_DIRECTORIES)
  set(MMESH_LIBRARIES mmesh::mmesh)
  return()
endif()

set(_MMESH_SEARCHES)

# Search MMESH_ROOT first if it is set.
if(MMESH_ROOT)
  set(_MMESH_SEARCH_ROOT PATHS ${MMESH_ROOT} NO_DEFAULT_PATH)
  list(APPEND _MMESH_SEARCHES _MMESH_SEARCH_ROOT)
endif()

list(APPEND _MMESH_SEARCHES _MMESH_SEARCH_NORMAL)

set(MMESH_NAMES mmesh)

# Try each search configuration.
foreach(search ${_MMESH_SEARCHES})
  find_path(MMESH_INCLUDE_DIR NAMES meshdecimation.h ${${search}} PATH_SUFFIXES include include/mmesh mmesh)
endforeach()

# Allow MMESH_LIBRARY to be set manually, as the location of the MMESH library
if(NOT MMESH_LIBRARY)
  foreach(search ${_MMESH_SEARCHES})
    find_library(MMESH_LIBRARY NAMES ${MMESH_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(MMESH_NAMES)

mark_as_advanced(MMESH_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MMESH REQUIRED_VARS MMESH_LIBRARY MMESH_INCLUDE_DIR)

if(MMESH_FOUND)
  set(MMESH_INCLUDE_DIRS ${MMESH_INCLUDE_DIR})

  if(NOT MMESH_LIBRARIES)
    set(MMESH_LIBRARIES ${MMESH_LIBRARY})
  endif()

  if(NOT TARGET mmesh::mmesh)
    add_library(mmesh::mmesh UNKNOWN IMPORTED)
    set_target_properties(
      mmesh::mmesh PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MMESH_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${MMESH_LIBRARY}"
    )
  endif()

  if(NOT TARGET mmesh::mmesh-static)
    add_library(mmesh::mmesh-static UNKNOWN IMPORTED)
    set_target_properties(
      mmesh::mmesh-static PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MMESH_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${MMESH_LIBRARY}"
    )
  endif()
endif()
