# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindVDS
--------

Find the native VDS includes and library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``VDS::VDS``, if
VDS has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

  VDS_INCLUDE_DIRS   - where to find vds.h, etc.
  VDS_LIBRARIES      - List of libraries when using vds.
  VDS_FOUND          - True if vds found.

Hints
^^^^^

A user may set ``VDS_ROOT`` to a vds installation root to tell this
module where to look.
#]=======================================================================]

set(_VDS_SEARCHES)

# Search VDS_ROOT first if it is set.
if(VDS_ROOT)
  set(_VDS_SEARCH_ROOT PATHS ${VDS_ROOT} NO_DEFAULT_PATH)
  list(APPEND _VDS_SEARCHES _VDS_SEARCH_ROOT)
endif()

# Normal search.
set(_VDS_x86 "(x86)")
set(_VDS_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/vds"
          "$ENV{ProgramFiles${_VDS_x86}}/vds")
unset(_VDS_x86)
list(APPEND _VDS_SEARCHES _VDS_SEARCH_NORMAL)

set(VDS_NAMES vds)

# Try each search configuration.
foreach(search ${_VDS_SEARCHES})
  find_path(VDS_INCLUDE_DIR NAMES vds.h ${${search}} PATH_SUFFIXES include include/vds vds)
endforeach()

# Allow VDS_LIBRARY to be set manually, as the location of the vds library
if(NOT VDS_LIBRARY)
  foreach(search ${_VDS_SEARCHES})
    find_library(VDS_LIBRARY NAMES ${VDS_NAMES} NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  endforeach()
endif()

unset(VDS_NAMES)

mark_as_advanced(VDS_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VDS REQUIRED_VARS VDS_LIBRARY VDS_INCLUDE_DIR)

if(VDS_FOUND)
    set(VDS_INCLUDE_DIRS ${VDS_INCLUDE_DIR})

    if(NOT VDS_LIBRARIES)
      set(VDS_LIBRARIES ${VDS_LIBRARY})
    endif()

    if(NOT TARGET VDS::VDS)
      add_library(VDS::VDS UNKNOWN IMPORTED)
      set_target_properties(VDS::VDS PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${VDS_INCLUDE_DIRS}")

      if(VDS_LIBRARY_RELEASE)
        set_property(TARGET VDS::VDS APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(VDS::VDS PROPERTIES
          IMPORTED_LOCATION_RELEASE "${VDS_LIBRARY_RELEASE}")
      endif()

      if(VDS_LIBRARY_DEBUG)
        set_property(TARGET VDS::VDS APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(VDS::VDS PROPERTIES
          IMPORTED_LOCATION_DEBUG "${VDS_LIBRARY_DEBUG}")
      endif()

      if(NOT VDS_LIBRARY_RELEASE AND NOT VDS_LIBRARY_DEBUG)
        set_property(TARGET VDS::VDS APPEND PROPERTY
          IMPORTED_LOCATION "${VDS_LIBRARY}")
      endif()
    endif()
endif()
