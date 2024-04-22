# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindMMESH
--------

Find the native MMESH includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``MMESH::MMESH``, if
MMESH has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  MMESH_INCLUDE_DIRS   - where to find meshdecimation.h, etc.
  MMESH_LIBRARIES      - List of libraries when using mmesh.
  MMESH_FOUND          - True if mmesh found.

Hints
^^^^^

A user may set ``MMESH_ROOT`` to a mmesh installation root to tell this
module where to look.
#]=======================================================================]

set(_MMESH_SEARCHES)

# Search MMESH_ROOT first if it is set.
if(MMESH_ROOT)
  set(_MMESH_SEARCH_ROOT PATHS ${MMESH_ROOT} NO_DEFAULT_PATH)
  list(APPEND _MMESH_SEARCHES _MMESH_SEARCH_ROOT)
endif()

set(MMESH_NAMES mmesh)

# Try each search configuration.
foreach(search ${_MMESH_SEARCHES})
	find_path(MMESH_INCLUDE_DIR NAMES meshdecimation.h ${${search}} PATH_SUFFIXES include include/mmesh mmesh)
endforeach()

# Allow MMESH_LIBRARY to be set manually, as the location of the openNURBS library
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

    if(NOT TARGET MMESH::MMESH)
      add_library(MMESH::MMESH UNKNOWN IMPORTED)
      set_target_properties(MMESH::MMESH PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MMESH_INCLUDE_DIRS}")

      if(MMESH_LIBRARY_RELEASE)
        set_property(TARGET MMESH::MMESH APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(MMESH::MMESH PROPERTIES
          IMPORTED_LOCATION_RELEASE "${MMESH_LIBRARY_RELEASE}")
      endif()

      if(MMESH_LIBRARY_DEBUG)
        set_property(TARGET MMESH::MMESH APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(MMESH::MMESH PROPERTIES
          IMPORTED_LOCATION_DEBUG "${MMESH_LIBRARY_DEBUG}")
      endif()

      if(NOT MMESH_LIBRARY_RELEASE AND NOT MMESH_LIBRARY_DEBUG)
        set_property(TARGET MMESH::MMESH APPEND PROPERTY
          IMPORTED_LOCATION "${MMESH_LIBRARY}")
      endif()
    endif()
endif()
