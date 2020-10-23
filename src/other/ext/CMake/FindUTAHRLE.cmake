# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindUTAHRLE
--------

Find the native UTAHRLE includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``UTAHRLE::UTAHRLE``, if
UTAHRLE has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  UTAHRLE_INCLUDE_DIRS   - where to find pam.h, etc.
  UTAHRLE_LIBRARIES      - List of libraries when using utahrle.
  UTAHRLE_FOUND          - True if utahrle found.

Hints
^^^^^

A user may set ``UTAHRLE_ROOT`` to a utahrle installation root to tell this
module where to look.
#]=======================================================================]

set(_UTAHRLE_SEARCHES)

# Search UTAHRLE_ROOT first if it is set.
if(UTAHRLE_ROOT)
  set(_UTAHRLE_SEARCH_ROOT PATHS ${UTAHRLE_ROOT} NO_DEFAULT_PATH)
  list(APPEND _UTAHRLE_SEARCHES _UTAHRLE_SEARCH_ROOT)
endif()

# Normal search.
set(_UTAHRLE_x86 "(x86)")
set(_UTAHRLE_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/utahrle"
          "$ENV{ProgramFiles${_UTAHRLE_x86}}/utahrle")
unset(_UTAHRLE_x86)
list(APPEND _UTAHRLE_SEARCHES _UTAHRLE_SEARCH_NORMAL)

set(UTAHRLE_NAMES utahrle)

# Try each search configuration.
foreach(search ${_UTAHRLE_SEARCHES})
  find_path(UTAHRLE_INCLUDE_DIR NAMES rle.h ${${search}} PATH_SUFFIXES include include/utahrle)
endforeach()

# Allow UTAHRLE_LIBRARY to be set manually, as the location of the utahrle library
if(NOT UTAHRLE_LIBRARY)
  foreach(search ${_UTAHRLE_SEARCHES})
    find_library(UTAHRLE_LIBRARY NAMES ${UTAHRLE_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(UTAHRLE_NAMES)

mark_as_advanced(UTAHRLE_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UTAHRLE REQUIRED_VARS UTAHRLE_LIBRARY UTAHRLE_INCLUDE_DIR)

if(UTAHRLE_FOUND)
    set(UTAHRLE_INCLUDE_DIRS ${UTAHRLE_INCLUDE_DIR})

    if(NOT UTAHRLE_LIBRARIES)
      set(UTAHRLE_LIBRARIES ${UTAHRLE_LIBRARY})
    endif()

    if(NOT TARGET UTAHRLE::UTAHRLE)
      add_library(UTAHRLE::UTAHRLE UNKNOWN IMPORTED)
      set_target_properties(UTAHRLE::UTAHRLE PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${UTAHRLE_INCLUDE_DIRS}")

      if(UTAHRLE_LIBRARY_RELEASE)
        set_property(TARGET UTAHRLE::UTAHRLE APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(UTAHRLE::UTAHRLE PROPERTIES
          IMPORTED_LOCATION_RELEASE "${UTAHRLE_LIBRARY_RELEASE}")
      endif()

      if(UTAHRLE_LIBRARY_DEBUG)
        set_property(TARGET UTAHRLE::UTAHRLE APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(UTAHRLE::UTAHRLE PROPERTIES
          IMPORTED_LOCATION_DEBUG "${UTAHRLE_LIBRARY_DEBUG}")
      endif()

      if(NOT UTAHRLE_LIBRARY_RELEASE AND NOT UTAHRLE_LIBRARY_DEBUG)
        set_property(TARGET UTAHRLE::UTAHRLE APPEND PROPERTY
          IMPORTED_LOCATION "${UTAHRLE_LIBRARY}")
      endif()
    endif()
endif()
