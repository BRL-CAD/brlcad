# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPOLY2TRI
--------

Find the native POLY2TRI includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``POLY2TRI::POLY2TRI``, if
POLY2TRI has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  POLY2TRI_INCLUDE_DIRS   - where to find poly2tri/poly2tri.h, etc.
  POLY2TRI_LIBRARIES      - List of libraries when using poly2tri.
  POLY2TRI_FOUND          - True if poly2tri found.

Hints
^^^^^

A user may set ``POLY2TRI_ROOT`` to a poly2tri installation root to tell this
module where to look.
#]=======================================================================]

set(_POLY2TRI_SEARCHES)

# Search POLY2TRI_ROOT first if it is set.
if(POLY2TRI_ROOT)
  set(_POLY2TRI_SEARCH_ROOT PATHS ${POLY2TRI_ROOT} NO_DEFAULT_PATH)
  list(APPEND _POLY2TRI_SEARCHES _POLY2TRI_SEARCH_ROOT)
endif()

# Normal search.
set(_POLY2TRI_x86 "(x86)")
set(_POLY2TRI_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/poly2tri"
          "$ENV{ProgramFiles${_POLY2TRI_x86}}/poly2tri")
unset(_POLY2TRI_x86)
list(APPEND _POLY2TRI_SEARCHES _POLY2TRI_SEARCH_NORMAL)

set(POLY2TRI_NAMES poly2tri)

# Try each search configuration.
foreach(search ${_POLY2TRI_SEARCHES})
  find_path(POLY2TRI_INCLUDE_DIR NAMES poly2tri/poly2tri.h ${${search}} PATH_SUFFIXES include include/poly2tri poly2tri)
endforeach()

# Allow POLY2TRI_LIBRARY to be set manually, as the location of the poly2tri library
if(NOT POLY2TRI_LIBRARY)
  foreach(search ${_POLY2TRI_SEARCHES})
    find_library(POLY2TRI_LIBRARY NAMES ${POLY2TRI_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(POLY2TRI_NAMES)

mark_as_advanced(POLY2TRI_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(POLY2TRI REQUIRED_VARS POLY2TRI_LIBRARY POLY2TRI_INCLUDE_DIR)

if(POLY2TRI_FOUND)
    set(POLY2TRI_INCLUDE_DIRS ${POLY2TRI_INCLUDE_DIR})

    if(NOT POLY2TRI_LIBRARIES)
      set(POLY2TRI_LIBRARIES ${POLY2TRI_LIBRARY})
    endif()

    if(NOT TARGET POLY2TRI::POLY2TRI)
      add_library(POLY2TRI::POLY2TRI UNKNOWN IMPORTED)
      set_target_properties(POLY2TRI::POLY2TRI PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${POLY2TRI_INCLUDE_DIRS}")

      if(POLY2TRI_LIBRARY_RELEASE)
        set_property(TARGET POLY2TRI::POLY2TRI APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(POLY2TRI::POLY2TRI PROPERTIES
          IMPORTED_LOCATION_RELEASE "${POLY2TRI_LIBRARY_RELEASE}")
      endif()

      if(POLY2TRI_LIBRARY_DEBUG)
        set_property(TARGET POLY2TRI::POLY2TRI APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(POLY2TRI::POLY2TRI PROPERTIES
          IMPORTED_LOCATION_DEBUG "${POLY2TRI_LIBRARY_DEBUG}")
      endif()

      if(NOT POLY2TRI_LIBRARY_RELEASE AND NOT POLY2TRI_LIBRARY_DEBUG)
        set_property(TARGET POLY2TRI::POLY2TRI APPEND PROPERTY
          IMPORTED_LOCATION "${POLY2TRI_LIBRARY}")
      endif()
    endif()
endif()
