# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindUV
--------

Find the native UV includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``UV::UV``, if
UV has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  UV_INCLUDE_DIRS   - where to find pam.h, etc.
  UV_LIBRARIES      - List of libraries when using uv.
  UV_FOUND          - True if uv found.

Hints
^^^^^

A user may set ``UV_ROOT`` to a uv installation root to tell this
module where to look.
#]=======================================================================]

set(_UV_SEARCHES)

# Search UV_ROOT first if it is set.
if(UV_ROOT)
  set(_UV_SEARCH_ROOT PATHS ${UV_ROOT} NO_DEFAULT_PATH)
  list(APPEND _UV_SEARCHES _UV_SEARCH_ROOT)
endif()

# Normal search.
set(_UV_x86 "(x86)")
set(_UV_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/uv"
          "$ENV{ProgramFiles${_UV_x86}}/uv")
unset(_UV_x86)
list(APPEND _UV_SEARCHES _UV_SEARCH_NORMAL)

set(UV_NAMES uv)

# Try each search configuration.
foreach(search ${_UV_SEARCHES})
  find_path(UV_INCLUDE_DIR NAMES uv.h ${${search}} PATH_SUFFIXES include include/uv uv)
endforeach()

# Allow UV_LIBRARY to be set manually, as the location of the uv library
if(NOT UV_LIBRARY)
  foreach(search ${_UV_SEARCHES})
    find_library(UV_LIBRARY NAMES ${UV_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(UV_NAMES)

mark_as_advanced(UV_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UV REQUIRED_VARS UV_LIBRARY UV_INCLUDE_DIR)

if(UV_FOUND)
    set(UV_INCLUDE_DIRS ${UV_INCLUDE_DIR})

    if(NOT UV_LIBRARIES)
      set(UV_LIBRARIES ${UV_LIBRARY})
    endif()

    if(NOT TARGET UV::UV)
      add_library(UV::UV UNKNOWN IMPORTED)
      set_target_properties(UV::UV PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${UV_INCLUDE_DIRS}")

      if(UV_LIBRARY_RELEASE)
        set_property(TARGET UV::UV APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(UV::UV PROPERTIES
          IMPORTED_LOCATION_RELEASE "${UV_LIBRARY_RELEASE}")
      endif()

      if(UV_LIBRARY_DEBUG)
        set_property(TARGET UV::UV APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(UV::UV PROPERTIES
          IMPORTED_LOCATION_DEBUG "${UV_LIBRARY_DEBUG}")
      endif()

      if(NOT UV_LIBRARY_RELEASE AND NOT UV_LIBRARY_DEBUG)
        set_property(TARGET UV::UV APPEND PROPERTY
          IMPORTED_LOCATION "${UV_LIBRARY}")
      endif()
    endif()
endif()
