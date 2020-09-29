# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindSPSR
--------

Find the native SPSR includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``SPSR::SPSR``, if
SPSR has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  SPSR_INCLUDE_DIRS   - where to find SPSR.h, etc.
  SPSR_LIBRARIES      - List of libraries when using spsr.
  SPSR_FOUND          - True if spsr found.

Hints
^^^^^

A user may set ``SPSR_ROOT`` to a spsr installation root to tell this
module where to look.
#]=======================================================================]

set(_SPSR_SEARCHES)

# Search SPSR_ROOT first if it is set.
if(SPSR_ROOT)
  set(_SPSR_SEARCH_ROOT PATHS ${SPSR_ROOT} NO_DEFAULT_PATH)
  list(APPEND _SPSR_SEARCHES _SPSR_SEARCH_ROOT)
endif()

# Normal search.
set(_SPSR_x86 "(x86)")
set(_SPSR_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/spsr"
          "$ENV{ProgramFiles${_SPSR_x86}}/spsr")
unset(_SPSR_x86)
list(APPEND _SPSR_SEARCHES _SPSR_SEARCH_NORMAL)

set(SPSR_NAMES spsr SPSR)

# Try each search configuration.
foreach(search ${_SPSR_SEARCHES})
	find_path(SPSR_INCLUDE_DIR NAMES SPSR.h ${${search}} PATH_SUFFIXES include include/SPSR SPSR)
endforeach()

# Allow SPSR_LIBRARY to be set manually, as the location of the spsr library
if(NOT SPSR_LIBRARY)
  foreach(search ${_SPSR_SEARCHES})
    find_library(SPSR_LIBRARY NAMES ${SPSR_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(SPSR_NAMES)

mark_as_advanced(SPSR_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPSR REQUIRED_VARS SPSR_LIBRARY SPSR_INCLUDE_DIR)

if(SPSR_FOUND)
    set(SPSR_INCLUDE_DIRS ${SPSR_INCLUDE_DIR})

    if(NOT SPSR_LIBRARIES)
      set(SPSR_LIBRARIES ${SPSR_LIBRARY})
    endif()

    if(NOT TARGET SPSR::SPSR)
      add_library(SPSR::SPSR UNKNOWN IMPORTED)
      set_target_properties(SPSR::SPSR PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SPSR_INCLUDE_DIRS}")

      if(SPSR_LIBRARY_RELEASE)
        set_property(TARGET SPSR::SPSR APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(SPSR::SPSR PROPERTIES
          IMPORTED_LOCATION_RELEASE "${SPSR_LIBRARY_RELEASE}")
      endif()

      if(SPSR_LIBRARY_DEBUG)
        set_property(TARGET SPSR::SPSR APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(SPSR::SPSR PROPERTIES
          IMPORTED_LOCATION_DEBUG "${SPSR_LIBRARY_DEBUG}")
      endif()

      if(NOT SPSR_LIBRARY_RELEASE AND NOT SPSR_LIBRARY_DEBUG)
        set_property(TARGET SPSR::SPSR APPEND PROPERTY
          IMPORTED_LOCATION "${SPSR_LIBRARY}")
      endif()
    endif()
endif()
