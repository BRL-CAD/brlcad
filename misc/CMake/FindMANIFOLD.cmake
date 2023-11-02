# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindMANIFOLD
--------

Find the native MANIFOLD includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``MANIFOLD::MANIFOLD``, if
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

set(_MANIFOLD_SEARCHES)

# Search MANIFOLD_ROOT first if it is set.
if(MANIFOLD_ROOT)
  set(_MANIFOLD_SEARCH_ROOT PATHS ${MANIFOLD_ROOT} NO_DEFAULT_PATH)
  list(APPEND _MANIFOLD_SEARCHES _MANIFOLD_SEARCH_ROOT)
endif()

set(MANIFOLD_NAMES manifold)

# Try each search configuration.
foreach(search ${_MANIFOLD_SEARCHES})
	find_path(MANIFOLD_INCLUDE_DIR NAMES manifold.h ${${search}} PATH_SUFFIXES include include/manifold)
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

    if(NOT TARGET MANIFOLD::MANIFOLD)
      add_library(MANIFOLD::MANIFOLD UNKNOWN IMPORTED)
      set_target_properties(MANIFOLD::MANIFOLD PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MANIFOLD_INCLUDE_DIRS}")

      if(MANIFOLD_LIBRARY_RELEASE)
        set_property(TARGET MANIFOLD::MANIFOLD APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(MANIFOLD::MANIFOLD PROPERTIES
          IMPORTED_LOCATION_RELEASE "${MANIFOLD_LIBRARY_RELEASE}")
      endif()

      if(MANIFOLD_LIBRARY_DEBUG)
        set_property(TARGET MANIFOLD::MANIFOLD APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(MANIFOLD::MANIFOLD PROPERTIES
          IMPORTED_LOCATION_DEBUG "${MANIFOLD_LIBRARY_DEBUG}")
      endif()

      if(NOT MANIFOLD_LIBRARY_RELEASE AND NOT MANIFOLD_LIBRARY_DEBUG)
        set_property(TARGET MANIFOLD::MANIFOLD APPEND PROPERTY
          IMPORTED_LOCATION "${MANIFOLD_LIBRARY}")
      endif()
    endif()
endif()
