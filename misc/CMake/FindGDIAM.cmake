# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindGDIAM
--------

Find the native GDIAM includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``GDIAM::GDIAM``, if
GDIAM has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  GDIAM_INCLUDE_DIRS   - where to find gdiam.hpp, etc.
  GDIAM_LIBRARIES      - List of libraries when using gdiam.
  GDIAM_FOUND          - True if gdiam found.

Hints
^^^^^

A user may set ``GDIAM_ROOT`` to a gdiam installation root to tell this
module where to look.
#]=======================================================================]

set(_GDIAM_SEARCHES)

# Search GDIAM_ROOT first if it is set.
if(GDIAM_ROOT)
  set(_GDIAM_SEARCH_ROOT PATHS ${GDIAM_ROOT} NO_DEFAULT_PATH)
  list(APPEND _GDIAM_SEARCHES _GDIAM_SEARCH_ROOT)
endif()

# Normal search.
set(_GDIAM_x86 "(x86)")
set(_GDIAM_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/gdiam"
          "$ENV{ProgramFiles${_GDIAM_x86}}/gdiam")
unset(_GDIAM_x86)
list(APPEND _GDIAM_SEARCHES _GDIAM_SEARCH_NORMAL)

set(GDIAM_NAMES gdiam)

# Try each search configuration.
foreach(search ${_GDIAM_SEARCHES})
  find_path(GDIAM_INCLUDE_DIR NAMES gdiam.hpp ${${search}} PATH_SUFFIXES include include/gdiam gdiam)
endforeach()

# Allow GDIAM_LIBRARY to be set manually, as the location of the gdiam library
if(NOT GDIAM_LIBRARY)
  foreach(search ${_GDIAM_SEARCHES})
    find_library(GDIAM_LIBRARY NAMES ${GDIAM_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(GDIAM_NAMES)

mark_as_advanced(GDIAM_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GDIAM REQUIRED_VARS GDIAM_LIBRARY GDIAM_INCLUDE_DIR)

if(GDIAM_FOUND)
    set(GDIAM_INCLUDE_DIRS ${GDIAM_INCLUDE_DIR})

    if(NOT GDIAM_LIBRARIES)
      set(GDIAM_LIBRARIES ${GDIAM_LIBRARY})
    endif()

    if(NOT TARGET GDIAM::GDIAM)
      add_library(GDIAM::GDIAM UNKNOWN IMPORTED)
      set_target_properties(GDIAM::GDIAM PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GDIAM_INCLUDE_DIRS}")

      if(GDIAM_LIBRARY_RELEASE)
        set_property(TARGET GDIAM::GDIAM APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(GDIAM::GDIAM PROPERTIES
          IMPORTED_LOCATION_RELEASE "${GDIAM_LIBRARY_RELEASE}")
      endif()

      if(GDIAM_LIBRARY_DEBUG)
        set_property(TARGET GDIAM::GDIAM APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(GDIAM::GDIAM PROPERTIES
          IMPORTED_LOCATION_DEBUG "${GDIAM_LIBRARY_DEBUG}")
      endif()

      if(NOT GDIAM_LIBRARY_RELEASE AND NOT GDIAM_LIBRARY_DEBUG)
        set_property(TARGET GDIAM::GDIAM APPEND PROPERTY
          IMPORTED_LOCATION "${GDIAM_LIBRARY}")
      endif()
    endif()
endif()
