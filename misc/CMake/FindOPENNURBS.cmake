# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindOPENNURBS
--------

Find the native OPENNURBS includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``OPENNURBS::OPENNURBS``, if
OPENNURBS has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  OPENNURBS_INCLUDE_DIRS   - where to find opennurbs.h, etc.
  OPENNURBS_LIBRARIES      - List of libraries when using openNURBS.
  OPENNURBS_FOUND          - True if openNURBS found.

Hints
^^^^^

A user may set ``OPENNURBS_ROOT`` to a openNURBS installation root to tell this
module where to look.
#]=======================================================================]

set(_OPENNURBS_SEARCHES)

# Search OPENNURBS_ROOT first if it is set.
if(OPENNURBS_ROOT)
  set(_OPENNURBS_SEARCH_ROOT PATHS ${OPENNURBS_ROOT} NO_DEFAULT_PATH)
  list(APPEND _OPENNURBS_SEARCHES _OPENNURBS_SEARCH_ROOT)
endif()

# Normal search.
set(_OPENNURBS_x86 "(x86)")
set(_OPENNURBS_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/openNURBS"
          "$ENV{ProgramFiles${_OPENNURBS_x86}}/openNURBS")
unset(_OPENNURBS_x86)
list(APPEND _OPENNURBS_SEARCHES _OPENNURBS_SEARCH_NORMAL)

set(OPENNURBS_NAMES openNURBS)

# Try each search configuration.
foreach(search ${_OPENNURBS_SEARCHES})
  find_path(OPENNURBS_INCLUDE_DIR NAMES opennurbs.h ${${search}} PATH_SUFFIXES include include/openNURBS openNURBS)
endforeach()

# Allow OPENNURBS_LIBRARY to be set manually, as the location of the openNURBS library
if(NOT OPENNURBS_LIBRARY)
  foreach(search ${_OPENNURBS_SEARCHES})
    find_library(OPENNURBS_LIBRARY NAMES ${OPENNURBS_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(OPENNURBS_NAMES)

mark_as_advanced(OPENNURBS_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENNURBS REQUIRED_VARS OPENNURBS_LIBRARY OPENNURBS_INCLUDE_DIR)

if(OPENNURBS_FOUND)
    set(OPENNURBS_INCLUDE_DIRS ${OPENNURBS_INCLUDE_DIR})

    if(NOT OPENNURBS_LIBRARIES)
      set(OPENNURBS_LIBRARIES ${OPENNURBS_LIBRARY})
    endif()

    if(NOT TARGET OPENNURBS::OPENNURBS)
      add_library(OPENNURBS::OPENNURBS UNKNOWN IMPORTED)
      set_target_properties(OPENNURBS::OPENNURBS PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OPENNURBS_INCLUDE_DIRS}")

      if(OPENNURBS_LIBRARY_RELEASE)
        set_property(TARGET OPENNURBS::OPENNURBS APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(OPENNURBS::OPENNURBS PROPERTIES
          IMPORTED_LOCATION_RELEASE "${OPENNURBS_LIBRARY_RELEASE}")
      endif()

      if(OPENNURBS_LIBRARY_DEBUG)
        set_property(TARGET OPENNURBS::OPENNURBS APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(OPENNURBS::OPENNURBS PROPERTIES
          IMPORTED_LOCATION_DEBUG "${OPENNURBS_LIBRARY_DEBUG}")
      endif()

      if(NOT OPENNURBS_LIBRARY_RELEASE AND NOT OPENNURBS_LIBRARY_DEBUG)
        set_property(TARGET OPENNURBS::OPENNURBS APPEND PROPERTY
          IMPORTED_LOCATION "${OPENNURBS_LIBRARY}")
      endif()
    endif()
endif()
