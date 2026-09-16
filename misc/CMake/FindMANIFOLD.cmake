# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindMANIFOLD
------------

Find the native Manifold includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``manifold::manifold``, if
MANIFOLD has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  MANIFOLD_INCLUDE_DIRS   - where to find manifold/manifold.h, etc.
  MANIFOLD_LIBRARIES      - List of libraries when using Manifold.
  MANIFOLD_FOUND          - True if Manifold found.

Hints
^^^^^

A user may set ``MANIFOLD_ROOT`` to a Manifold installation root to tell this
module where to look.
#]=======================================================================]

find_package(manifold CONFIG QUIET)
if(TARGET manifold::manifold)
  set(MANIFOLD_FOUND TRUE)
  get_target_property(MANIFOLD_INCLUDE_DIRS manifold::manifold INTERFACE_INCLUDE_DIRECTORIES)
  set(MANIFOLD_LIBRARIES manifold::manifold)
  return()
endif()

set(_MANIFOLD_SEARCHES)

# Search MANIFOLD_ROOT first if it is set.
if(MANIFOLD_ROOT)
  set(_MANIFOLD_SEARCH_ROOT PATHS ${MANIFOLD_ROOT} NO_DEFAULT_PATH)
  list(APPEND _MANIFOLD_SEARCHES _MANIFOLD_SEARCH_ROOT)
endif()

list(APPEND _MANIFOLD_SEARCHES _MANIFOLD_SEARCH_NORMAL)

set(MANIFOLD_NAMES manifold)

# Try each search configuration.
foreach(search ${_MANIFOLD_SEARCHES})
  find_path(MANIFOLD_INCLUDE_DIR NAMES manifold.h ${${search}} PATH_SUFFIXES include include/manifold manifold)
endforeach()

# Allow MANIFOLD_LIBRARY to be set manually, as the location of the Manifold library
if(NOT MANIFOLD_LIBRARY)
  foreach(search ${_MANIFOLD_SEARCHES})
    find_library(MANIFOLD_LIBRARY NAMES ${MANIFOLD_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(MANIFOLD_NAMES)

mark_as_advanced(MANIFOLD_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MANIFOLD REQUIRED_VARS MANIFOLD_LIBRARY MANIFOLD_INCLUDE_DIR)

if(MANIFOLD_FOUND)
  set(MANIFOLD_INCLUDE_DIRS ${MANIFOLD_INCLUDE_DIR})

  if(NOT MANIFOLD_LIBRARIES)
    set(MANIFOLD_LIBRARIES ${MANIFOLD_LIBRARY})
  endif()

  if(NOT TARGET manifold::manifold)
    add_library(manifold::manifold UNKNOWN IMPORTED)
    set_target_properties(
      manifold::manifold PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MANIFOLD_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${MANIFOLD_LIBRARY}"
    )
  endif()

  if(NOT TARGET manifold::manifold-static)
    add_library(manifold::manifold-static UNKNOWN IMPORTED)
    set_target_properties(
      manifold::manifold-static PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MANIFOLD_INCLUDE_DIRS}"
      IMPORTED_LOCATION "${MANIFOLD_LIBRARY}"
    )
  endif()
endif()
