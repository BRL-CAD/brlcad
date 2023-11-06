# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindOSMESA
--------

Find the native OSMESA includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``OSMESA::OSMESA``, if
OSMESA has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  OSMESA_INCLUDE_DIRS   - where to find osmesa.h, etc.
  OSMESA_LIBRARIES      - List of libraries when using openNURBS.
  OSMESA_FOUND          - True if openNURBS found.

Hints
^^^^^

A user may set ``OSMESA_ROOT`` to a openNURBS installation root to tell this
module where to look.
#]=======================================================================]

set(_OSMESA_SEARCHES)

# Search OSMESA_ROOT first if it is set.
if(OSMESA_ROOT)
  set(_OSMESA_SEARCH_ROOT PATHS ${OSMESA_ROOT} NO_DEFAULT_PATH)
  list(APPEND _OSMESA_SEARCHES _OSMESA_SEARCH_ROOT)
endif()

set(OSMESA_NAMES osmesa OSMesa)

# Try each search configuration.
foreach(search ${_OSMESA_SEARCHES})
	find_path(OSMESA_INCLUDE_DIR NAMES osmesa.h ${${search}} PATH_SUFFIXES include include/OSMesa OSMesa)
endforeach()

# Allow OSMESA_LIBRARY to be set manually, as the location of the openNURBS library
if(NOT OSMESA_LIBRARY)
  foreach(search ${_OSMESA_SEARCHES})
    find_library(OSMESA_LIBRARY NAMES ${OSMESA_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(OSMESA_NAMES)

mark_as_advanced(OSMESA_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OSMESA REQUIRED_VARS OSMESA_LIBRARY OSMESA_INCLUDE_DIR)

if(OSMESA_FOUND)
    set(OSMESA_INCLUDE_DIRS ${OSMESA_INCLUDE_DIR})

    if(NOT OSMESA_LIBRARIES)
      set(OSMESA_LIBRARIES ${OSMESA_LIBRARY})
    endif()

    if(NOT TARGET OSMESA::OSMESA)
      add_library(OSMESA::OSMESA UNKNOWN IMPORTED)
      set_target_properties(OSMESA::OSMESA PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OSMESA_INCLUDE_DIRS}")

      if(OSMESA_LIBRARY_RELEASE)
        set_property(TARGET OSMESA::OSMESA APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(OSMESA::OSMESA PROPERTIES
          IMPORTED_LOCATION_RELEASE "${OSMESA_LIBRARY_RELEASE}")
      endif()

      if(OSMESA_LIBRARY_DEBUG)
        set_property(TARGET OSMESA::OSMESA APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(OSMESA::OSMESA PROPERTIES
          IMPORTED_LOCATION_DEBUG "${OSMESA_LIBRARY_DEBUG}")
      endif()

      if(NOT OSMESA_LIBRARY_RELEASE AND NOT OSMESA_LIBRARY_DEBUG)
        set_property(TARGET OSMESA::OSMESA APPEND PROPERTY
          IMPORTED_LOCATION "${OSMESA_LIBRARY}")
      endif()
    endif()
endif()
