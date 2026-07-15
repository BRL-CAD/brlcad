# Distributed under the OSI-approved BSD 3-Clause License.
# See CMake's LICENSE.rst or https://cmake.org/licensing for details.
#
# BRL-CAD local variant:
# - Based on the modern CMake FindZLIB module structure.
# - Preserves BRL-CAD's custom zlib library names, including z_brl.
# - Honors ZLIB_ROOT and ENV{ZLIB_ROOT} as installation-root hints.
# - Keeps the traditional ZLIB::ZLIB target.
# - Adds optional variant targets when they can be found:
#     ZLIB::ZLIB_SHARED
#     ZLIB::ZLIB_STATIC
#     ZLIB::ZLIBSTATIC  (alias-compatible with upstream zlib's config package)
#
#[=======================================================================[.rst:
FindZLIB
--------

Finds the native zlib data compression library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, when found:

``ZLIB::ZLIB``
  Compatibility/default target that encapsulates the selected zlib usage
  requirements.  Selection follows ``ZLIB_USE_STATIC_LIBS`` when set.

``ZLIB::ZLIB_SHARED``
  Shared zlib target, when a shared zlib library can be identified.

``ZLIB::ZLIB_STATIC``
  Static zlib target, when a static zlib library can be identified.

``ZLIB::ZLIBSTATIC``
  Alias to ``ZLIB::ZLIB_STATIC`` when the static target is available.  This
  matches the target name used by zlib's own CMake package.

Result Variables
^^^^^^^^^^^^^^^^

``ZLIB_FOUND``
  Boolean indicating whether zlib was found.

``ZLIB_VERSION``
  The version of zlib found.

``ZLIB_INCLUDE_DIRS``
  Include directories containing ``zlib.h`` and other headers needed to use
  zlib.

``ZLIB_LIBRARIES``
  List of libraries needed to link to the default/selected zlib.

``ZLIB_SHARED_FOUND``
  Boolean indicating whether a shared zlib library was found.

``ZLIB_STATIC_FOUND``
  Boolean indicating whether a static zlib library was found.

``ZLIB_SHARED_LIBRARIES``
  Shared zlib library or debug/optimized library list, when found.

``ZLIB_STATIC_LIBRARIES``
  Static zlib library or debug/optimized library list, when found.

Hints
^^^^^

``ZLIB_ROOT``
  A user may set this variable to a zlib installation root.

``ENV{ZLIB_ROOT}``
  Environment-variable form of ``ZLIB_ROOT``.

``ZLIB_USE_STATIC_LIBS``
  Set to ``ON`` before calling ``find_package(ZLIB)`` to prefer static
  libraries for the compatibility/default ``ZLIB::ZLIB`` target.

``ZLIB_NAMES`` and ``ZLIB_NAMES_DEBUG``
  Optional project override for the default selected-library search names.

``ZLIB_SHARED_NAMES`` and ``ZLIB_SHARED_NAMES_DEBUG``
  Optional project override for shared-library search names.

``ZLIB_STATIC_NAMES`` and ``ZLIB_STATIC_NAMES_DEBUG``
  Optional project override for static-library search names.

The default name lists include BRL-CAD's custom ``z_brl`` name.
#]=======================================================================]

cmake_policy(PUSH)
if(POLICY CMP0159)
  cmake_policy(SET CMP0159 NEW) # file(STRINGS) with REGEX updates CMAKE_MATCH_<n>
endif()

set(_ZLIB_SEARCHES)

# Search ZLIB_ROOT first if it is set.  Support both a CMake variable and
# an environment variable for command-line and packaged-build workflows.
if(ZLIB_ROOT)
  set(_ZLIB_SEARCH_ROOT PATHS "${ZLIB_ROOT}" NO_DEFAULT_PATH)
  list(APPEND _ZLIB_SEARCHES _ZLIB_SEARCH_ROOT)
elseif(DEFINED ENV{ZLIB_ROOT} AND NOT "$ENV{ZLIB_ROOT}" STREQUAL "")
  set(_ZLIB_SEARCH_ROOT PATHS "$ENV{ZLIB_ROOT}" NO_DEFAULT_PATH)
  list(APPEND _ZLIB_SEARCHES _ZLIB_SEARCH_ROOT)
endif()

# Normal search.
set(_ZLIB_x86 "(x86)")
set(_ZLIB_SEARCH_NORMAL
  PATHS
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]"
    "$ENV{ProgramFiles}/zlib"
    "$ENV{ProgramFiles${_ZLIB_x86}}/zlib"
)
unset(_ZLIB_x86)
list(APPEND _ZLIB_SEARCHES _ZLIB_SEARCH_NORMAL)

# BRL-CAD uses z_brl for its bundled zlib.  Keep that name first so an
# explicitly supplied root or local prefix finds the BRL-CAD build before a
# generic system zlib.
set(_ZLIB_DEFAULT_SHARED_NAMES
  z_brl
  z
  zlib
  zdll
  zlib1
  zlibstatic
  zlibwapi
  zlibvc
  zlibstat
)
set(_ZLIB_DEFAULT_SHARED_NAMES_DEBUG
  z_brld
  zd
  zlibd
  zdlld
  zlibd1
  zlib1d
  zlibstaticd
  zlibwapid
  zlibvcd
  zlibstatd
)

set(_ZLIB_DEFAULT_STATIC_NAMES
  # On Windows the import library for the shared zlib and the static library
  # share the .lib suffix, so the static name must come first and be distinct
  # from the shared import lib (z_brl.lib) or find_library would pick the import
  # lib and keep the z_brl.dll runtime dependency.  BRL-CAD's static zlib is
  # z_brl-static.lib.
  z_brl-static
  z_brl
  z_brl_static
  z_brlstatic
  zs
  zlibstatic
  zlibstat
  zlib
  z
)
set(_ZLIB_DEFAULT_STATIC_NAMES_DEBUG
  z_brld
  z_brld_static
  z_brldstatic
  zsd
  zlibstaticd
  zlibstatd
  zlibd
  zd
)

if(NOT ZLIB_SHARED_NAMES)
  set(ZLIB_SHARED_NAMES ${_ZLIB_DEFAULT_SHARED_NAMES})
endif()
if(NOT ZLIB_SHARED_NAMES_DEBUG)
  set(ZLIB_SHARED_NAMES_DEBUG ${_ZLIB_DEFAULT_SHARED_NAMES_DEBUG})
endif()

if(NOT ZLIB_STATIC_NAMES)
  set(ZLIB_STATIC_NAMES ${_ZLIB_DEFAULT_STATIC_NAMES})
endif()
if(NOT ZLIB_STATIC_NAMES_DEBUG)
  set(ZLIB_STATIC_NAMES_DEBUG ${_ZLIB_DEFAULT_STATIC_NAMES_DEBUG})
endif()

if(NOT ZLIB_NAMES)
  if(ZLIB_USE_STATIC_LIBS)
    set(ZLIB_NAMES ${ZLIB_STATIC_NAMES})
  else()
    set(ZLIB_NAMES ${ZLIB_SHARED_NAMES})
  endif()
endif()
if(NOT ZLIB_NAMES_DEBUG)
  if(ZLIB_USE_STATIC_LIBS)
    set(ZLIB_NAMES_DEBUG ${ZLIB_STATIC_NAMES_DEBUG})
  else()
    set(ZLIB_NAMES_DEBUG ${ZLIB_SHARED_NAMES_DEBUG})
  endif()
endif()

foreach(search ${_ZLIB_SEARCHES})
  find_path(ZLIB_INCLUDE_DIR
    NAMES zlib.h
    ${${search}}
    PATH_SUFFIXES include
  )
endforeach()

function(_zlib_save_find_library_state)
  foreach(v CMAKE_FIND_LIBRARY_PREFIXES CMAKE_FIND_LIBRARY_SUFFIXES)
    if(DEFINED ${v})
      set(_zlib_ORIG_${v} "${${v}}" PARENT_SCOPE)
      set(_zlib_HAD_${v} TRUE PARENT_SCOPE)
    else()
      set(_zlib_ORIG_${v} PARENT_SCOPE)
      set(_zlib_HAD_${v} FALSE PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

macro(_zlib_restore_find_library_state)
  foreach(v CMAKE_FIND_LIBRARY_PREFIXES CMAKE_FIND_LIBRARY_SUFFIXES)
    if(_zlib_HAD_${v})
      set(${v} "${_zlib_ORIG_${v}}")
    else()
      unset(${v})
    endif()
  endforeach()
endmacro()

function(_zlib_find_variant prefix libtype)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs NAMES NAMES_DEBUG)
  cmake_parse_arguments(_ZLIB_FIND "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  _zlib_save_find_library_state()

  # Support the MinGW/win32 Makefile.gcc build style.
  if(WIN32)
    list(APPEND CMAKE_FIND_LIBRARY_PREFIXES "" "lib")
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a")
  endif()

  if(libtype STREQUAL "STATIC")
    if(WIN32)
      set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    else()
      set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    endif()
  elseif(libtype STREQUAL "SHARED")
    if(WIN32)
      # On Windows, shared zlib is normally discovered through an import
      # library.  Distinguishing a .lib import library from a static .lib is
      # not reliable by suffix alone, so the name list does most of the work.
      set(CMAKE_FIND_LIBRARY_SUFFIXES .dll.a .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
    elseif(APPLE)
      set(CMAKE_FIND_LIBRARY_SUFFIXES .dylib .so ${CMAKE_FIND_LIBRARY_SUFFIXES})
    elseif(UNIX)
      set(CMAKE_FIND_LIBRARY_SUFFIXES .so ${CMAKE_FIND_LIBRARY_SUFFIXES})
    endif()
  endif()

  foreach(search ${_ZLIB_SEARCHES})
    find_library(${prefix}_LIBRARY_RELEASE
      NAMES ${_ZLIB_FIND_NAMES}
      NAMES_PER_DIR
      ${${search}}
      PATH_SUFFIXES lib lib64
    )
    find_library(${prefix}_LIBRARY_DEBUG
      NAMES ${_ZLIB_FIND_NAMES_DEBUG}
      NAMES_PER_DIR
      ${${search}}
      PATH_SUFFIXES lib lib64
    )
  endforeach()

  _zlib_restore_find_library_state()

  set(${prefix}_LIBRARY_RELEASE "${${prefix}_LIBRARY_RELEASE}" PARENT_SCOPE)
  set(${prefix}_LIBRARY_DEBUG "${${prefix}_LIBRARY_DEBUG}" PARENT_SCOPE)
endfunction()

# Main/default search.  Preserve CMake's behavior of allowing ZLIB_LIBRARY to
# be set manually as the selected library.
if(NOT ZLIB_LIBRARY)
  _zlib_find_variant(ZLIB UNKNOWN
    NAMES ${ZLIB_NAMES}
    NAMES_DEBUG ${ZLIB_NAMES_DEBUG}
  )

  include(SelectLibraryConfigurations)
  select_library_configurations(ZLIB)
endif()

# Variant searches.  These do not override the selected/default ZLIB_LIBRARY.
if(NOT ZLIB_SHARED_LIBRARY)
  _zlib_find_variant(ZLIB_SHARED SHARED
    NAMES ${ZLIB_SHARED_NAMES}
    NAMES_DEBUG ${ZLIB_SHARED_NAMES_DEBUG}
  )
  include(SelectLibraryConfigurations)
  select_library_configurations(ZLIB_SHARED)
endif()

if(NOT ZLIB_STATIC_LIBRARY)
  _zlib_find_variant(ZLIB_STATIC STATIC
    NAMES ${ZLIB_STATIC_NAMES}
    NAMES_DEBUG ${ZLIB_STATIC_NAMES_DEBUG}
  )
  include(SelectLibraryConfigurations)
  select_library_configurations(ZLIB_STATIC)
endif()

unset(ZLIB_NAMES)
unset(ZLIB_NAMES_DEBUG)
unset(_ZLIB_DEFAULT_SHARED_NAMES)
unset(_ZLIB_DEFAULT_SHARED_NAMES_DEBUG)
unset(_ZLIB_DEFAULT_STATIC_NAMES)
unset(_ZLIB_DEFAULT_STATIC_NAMES_DEBUG)

mark_as_advanced(
  ZLIB_INCLUDE_DIR
  ZLIB_LIBRARY
  ZLIB_LIBRARY_RELEASE
  ZLIB_LIBRARY_DEBUG
  ZLIB_SHARED_LIBRARY
  ZLIB_SHARED_LIBRARY_RELEASE
  ZLIB_SHARED_LIBRARY_DEBUG
  ZLIB_STATIC_LIBRARY
  ZLIB_STATIC_LIBRARY_RELEASE
  ZLIB_STATIC_LIBRARY_DEBUG
)

if(ZLIB_INCLUDE_DIR AND EXISTS "${ZLIB_INCLUDE_DIR}/zlib.h")
  file(STRINGS "${ZLIB_INCLUDE_DIR}/zlib.h" ZLIB_H
    REGEX "^#define ZLIB_VERSION \"[^\"]*\"$"
  )

  if(ZLIB_H MATCHES "ZLIB_VERSION \"(([0-9]+)\\.([0-9]+)(\\.([0-9]+)(\\.([0-9]+))?)?)")
    set(ZLIB_VERSION_STRING "${CMAKE_MATCH_1}")
    set(ZLIB_VERSION_MAJOR "${CMAKE_MATCH_2}")
    set(ZLIB_VERSION_MINOR "${CMAKE_MATCH_3}")
    set(ZLIB_VERSION_PATCH "${CMAKE_MATCH_5}")
    set(ZLIB_VERSION_TWEAK "${CMAKE_MATCH_7}")
  else()
    set(ZLIB_VERSION_STRING "")
    set(ZLIB_VERSION_MAJOR "")
    set(ZLIB_VERSION_MINOR "")
    set(ZLIB_VERSION_PATCH "")
    set(ZLIB_VERSION_TWEAK "")
  endif()

  set(ZLIB_MAJOR_VERSION "${ZLIB_VERSION_MAJOR}")
  set(ZLIB_MINOR_VERSION "${ZLIB_VERSION_MINOR}")
  set(ZLIB_PATCH_VERSION "${ZLIB_VERSION_PATCH}")
  set(ZLIB_VERSION "${ZLIB_VERSION_STRING}")
endif()

set(ZLIB_SHARED_FOUND FALSE)
if(ZLIB_INCLUDE_DIR AND ZLIB_SHARED_LIBRARY)
  set(ZLIB_SHARED_FOUND TRUE)
  set(ZLIB_SHARED_LIBRARIES ${ZLIB_SHARED_LIBRARY})
endif()

set(ZLIB_STATIC_FOUND FALSE)
if(ZLIB_INCLUDE_DIR AND ZLIB_STATIC_LIBRARY)
  set(ZLIB_STATIC_FOUND TRUE)
  set(ZLIB_STATIC_LIBRARIES ${ZLIB_STATIC_LIBRARY})
endif()

# Component support:
#   find_package(ZLIB COMPONENTS shared)
#   find_package(ZLIB COMPONENTS static)
#
# The compatibility/no-component case keeps the classic single-result behavior.
set(_ZLIB_REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
foreach(_zlib_component IN LISTS ZLIB_FIND_COMPONENTS)
  if(_zlib_component STREQUAL "shared")
    if(ZLIB_SHARED_FOUND)
      set(ZLIB_shared_FOUND TRUE)
    else()
      set(ZLIB_shared_FOUND FALSE)
    endif()
    list(APPEND _ZLIB_REQUIRED_VARS ZLIB_SHARED_LIBRARY)
  elseif(_zlib_component STREQUAL "static")
    if(ZLIB_STATIC_FOUND)
      set(ZLIB_static_FOUND TRUE)
    else()
      set(ZLIB_static_FOUND FALSE)
    endif()
    list(APPEND _ZLIB_REQUIRED_VARS ZLIB_STATIC_LIBRARY)
  else()
    set(ZLIB_${_zlib_component}_FOUND FALSE)
  endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB
  REQUIRED_VARS ${_ZLIB_REQUIRED_VARS}
  VERSION_VAR ZLIB_VERSION
  HANDLE_COMPONENTS
)

if(ZLIB_FOUND)
  set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})

  if(NOT ZLIB_LIBRARIES)
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
  endif()

  if(ZLIB_SHARED_FOUND AND NOT TARGET ZLIB::ZLIB_SHARED)
    add_library(ZLIB::ZLIB_SHARED UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB_SHARED PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
      IMPORTED_NO_SYSTEM TRUE
    )

    if(ZLIB_SHARED_LIBRARY_RELEASE)
      set_property(TARGET ZLIB::ZLIB_SHARED APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE
      )
      set_target_properties(ZLIB::ZLIB_SHARED PROPERTIES
        IMPORTED_LOCATION_RELEASE "${ZLIB_SHARED_LIBRARY_RELEASE}"
      )
    endif()

    if(ZLIB_SHARED_LIBRARY_DEBUG)
      set_property(TARGET ZLIB::ZLIB_SHARED APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG
      )
      set_target_properties(ZLIB::ZLIB_SHARED PROPERTIES
        IMPORTED_LOCATION_DEBUG "${ZLIB_SHARED_LIBRARY_DEBUG}"
      )
    endif()

    if(NOT ZLIB_SHARED_LIBRARY_RELEASE AND NOT ZLIB_SHARED_LIBRARY_DEBUG)
      set_target_properties(ZLIB::ZLIB_SHARED PROPERTIES
        IMPORTED_LOCATION "${ZLIB_SHARED_LIBRARY}"
      )
    endif()
  endif()

  if(ZLIB_STATIC_FOUND AND NOT TARGET ZLIB::ZLIB_STATIC)
    add_library(ZLIB::ZLIB_STATIC STATIC IMPORTED)
    set_target_properties(ZLIB::ZLIB_STATIC PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
      IMPORTED_NO_SYSTEM TRUE
    )

    if(ZLIB_STATIC_LIBRARY_RELEASE)
      set_property(TARGET ZLIB::ZLIB_STATIC APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE
      )
      set_target_properties(ZLIB::ZLIB_STATIC PROPERTIES
        IMPORTED_LOCATION_RELEASE "${ZLIB_STATIC_LIBRARY_RELEASE}"
      )
    endif()

    if(ZLIB_STATIC_LIBRARY_DEBUG)
      set_property(TARGET ZLIB::ZLIB_STATIC APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG
      )
      set_target_properties(ZLIB::ZLIB_STATIC PROPERTIES
        IMPORTED_LOCATION_DEBUG "${ZLIB_STATIC_LIBRARY_DEBUG}"
      )
    endif()

    if(NOT ZLIB_STATIC_LIBRARY_RELEASE AND NOT ZLIB_STATIC_LIBRARY_DEBUG)
      set_target_properties(ZLIB::ZLIB_STATIC PROPERTIES
        IMPORTED_LOCATION "${ZLIB_STATIC_LIBRARY}"
      )
    endif()
  endif()

  if(TARGET ZLIB::ZLIB_STATIC AND NOT TARGET ZLIB::ZLIBSTATIC)
    add_library(ZLIB::ZLIBSTATIC ALIAS ZLIB::ZLIB_STATIC)
  endif()

  if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}"
      IMPORTED_NO_SYSTEM TRUE
    )

    if(ZLIB_LIBRARY_RELEASE)
      set_property(TARGET ZLIB::ZLIB APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE
      )
      set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION_RELEASE "${ZLIB_LIBRARY_RELEASE}"
      )
    endif()

    if(ZLIB_LIBRARY_DEBUG)
      set_property(TARGET ZLIB::ZLIB APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG
      )
      set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION_DEBUG "${ZLIB_LIBRARY_DEBUG}"
      )
    endif()

    if(NOT ZLIB_LIBRARY_RELEASE AND NOT ZLIB_LIBRARY_DEBUG)
      set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION "${ZLIB_LIBRARY}"
      )
    endif()
  endif()
endif()

unset(_ZLIB_REQUIRED_VARS)
unset(_zlib_component)
unset(_ZLIB_SEARCHES)
unset(_ZLIB_SEARCH_ROOT)
unset(_ZLIB_SEARCH_NORMAL)

cmake_policy(POP)
