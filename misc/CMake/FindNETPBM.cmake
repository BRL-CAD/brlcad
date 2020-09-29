# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindNETPBM
--------

Find the native NETPBM includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``NETPBM::NETPBM``, if
NETPBM has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  NETPBM_INCLUDE_DIRS   - where to find pam.h, etc.
  NETPBM_LIBRARIES      - List of libraries when using netpbm.
  NETPBM_FOUND          - True if netpbm found.

Hints
^^^^^

A user may set ``NETPBM_ROOT`` to a netpbm installation root to tell this
module where to look.
#]=======================================================================]

set(_NETPBM_SEARCHES)

# Search NETPBM_ROOT first if it is set.
if(NETPBM_ROOT)
  set(_NETPBM_SEARCH_ROOT PATHS ${NETPBM_ROOT} NO_DEFAULT_PATH)
  list(APPEND _NETPBM_SEARCHES _NETPBM_SEARCH_ROOT)
endif()

# Normal search.
set(_NETPBM_x86 "(x86)")
set(_NETPBM_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/netpbm"
          "$ENV{ProgramFiles${_NETPBM_x86}}/netpbm")
unset(_NETPBM_x86)
list(APPEND _NETPBM_SEARCHES _NETPBM_SEARCH_NORMAL)

set(NETPBM_NAMES netpbm)

# Try each search configuration.
foreach(search ${_NETPBM_SEARCHES})
  find_path(NETPBM_INCLUDE_DIR NAMES pam.h ${${search}} PATH_SUFFIXES include include/netpbm netpbm)
endforeach()

# Allow NETPBM_LIBRARY to be set manually, as the location of the netpbm library
if(NOT NETPBM_LIBRARY)
  foreach(search ${_NETPBM_SEARCHES})
    find_library(NETPBM_LIBRARY NAMES ${NETPBM_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(NETPBM_NAMES)

mark_as_advanced(NETPBM_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NETPBM REQUIRED_VARS NETPBM_LIBRARY NETPBM_INCLUDE_DIR)

if(NETPBM_FOUND)
    set(NETPBM_INCLUDE_DIRS ${NETPBM_INCLUDE_DIR})

    if(NOT NETPBM_LIBRARIES)
      set(NETPBM_LIBRARIES ${NETPBM_LIBRARY})
    endif()

    if(NOT TARGET NETPBM::NETPBM)
      add_library(NETPBM::NETPBM UNKNOWN IMPORTED)
      set_target_properties(NETPBM::NETPBM PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NETPBM_INCLUDE_DIRS}")

      if(NETPBM_LIBRARY_RELEASE)
        set_property(TARGET NETPBM::NETPBM APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(NETPBM::NETPBM PROPERTIES
          IMPORTED_LOCATION_RELEASE "${NETPBM_LIBRARY_RELEASE}")
      endif()

      if(NETPBM_LIBRARY_DEBUG)
        set_property(TARGET NETPBM::NETPBM APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(NETPBM::NETPBM PROPERTIES
          IMPORTED_LOCATION_DEBUG "${NETPBM_LIBRARY_DEBUG}")
      endif()

      if(NOT NETPBM_LIBRARY_RELEASE AND NOT NETPBM_LIBRARY_DEBUG)
        set_property(TARGET NETPBM::NETPBM APPEND PROPERTY
          IMPORTED_LOCATION "${NETPBM_LIBRARY}")
      endif()
    endif()
endif()
