#                F I N D B R L C A D . C M A K E
# BRL-CAD
#
# Copyright (c) 2024-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
# @file FindBRLCAD.cmake
#
# Compatibility shim: try the modern Config-file package first, then
# fall back to the legacy probe-based search for older installs.
#
# Defines on success:
#   BRLCAD_FOUND            - TRUE
#   BRLCAD_VERSION          - version string "MAJOR.MINOR.PATCH"
#   BRLCAD_INCLUDE_DIRS     - include directories
#   BRLCAD_LIBRARIES        - aggregate link target (BRLCAD::brlcad)
#   BRLCAD_<UPPER>_LIBRARY  - per-lib imported target, e.g. BRLCAD_RT_LIBRARY
#
# Modern target surface (preferred):
#   BRLCAD::bu  BRLCAD::bn  BRLCAD::bg  BRLCAD::bv  BRLCAD::nmg
#   BRLCAD::brep  BRLCAD::rt  BRLCAD::wdb  BRLCAD::ged  BRLCAD::analyze
#   BRLCAD::gcv  BRLCAD::icv  BRLCAD::dm  BRLCAD::fft  BRLCAD::optical
#   BRLCAD::pkg  BRLCAD::render  BRLCAD::brlcad (aggregate)
#
# DEPRECATED: This Find module exists for compatibility with projects
#   that install FindBRLCAD.cmake into their own source tree.  New
#   projects should use find_package(BRLCAD CONFIG ...) directly so
#   that CMake locates BRLCADConfig.cmake from the BRL-CAD install.
#   This shim will be removed in BRL-CAD 8.0.
#
#########################################################################

# ------------------------------------------------------------------
# 1.  Try the Config-file package first (BRLCADConfig.cmake installed
#     under <prefix>/lib/cmake/BRLCAD/).
# ------------------------------------------------------------------

# Build a list of hints so we honour BRLCAD_ROOT if set
set(_brlcad_config_hints)
if(BRLCAD_ROOT)
  list(APPEND _brlcad_config_hints "${BRLCAD_ROOT}")
elseif(DEFINED ENV{BRLCAD_ROOT})
  list(APPEND _brlcad_config_hints "$ENV{BRLCAD_ROOT}")
endif()

# Propagate COMPONENTS and version requirement to the Config search
set(_brlcad_ver_req)
if(BRLCAD_FIND_VERSION)
  set(_brlcad_ver_req ${BRLCAD_FIND_VERSION})
endif()

find_package(
  BRLCAD ${_brlcad_ver_req}
  CONFIG QUIET
  HINTS ${_brlcad_config_hints}
  # Also look under the versioned /usr/brlcad/* directories
  PATHS
    /usr/brlcad
    /usr/local/brlcad
  PATH_SUFFIXES
    rel-${BRLCAD_FIND_VERSION}
    rel-${_brlcad_ver_req}
)

if(BRLCAD_FOUND)
  # Config package succeeded - all variables and targets are already set
  # by BRLCADConfig.cmake.  Just forward QUIET/REQUIRED handling.
  if(NOT BRLCAD_FIND_QUIETLY)
    message(STATUS "Found BRLCAD ${BRLCAD_VERSION} (Config) at ${BRLCAD_LIB_DIR}")
  endif()
  return()
endif()

# ------------------------------------------------------------------
# 2.  Legacy probe-based fallback for pre-Config BRL-CAD installs
# ------------------------------------------------------------------

if(NOT DEFINED BRLCAD_ROOT)
  set(BRLCAD_ROOT "$ENV{BRLCAD_ROOT}")
endif()

# First, find the install directories.
if(NOT BRLCAD_ROOT)
  # Try looking for BRL-CAD's brlcad-config - it should give us
  # a location for bin, and the parent will be the root dir
  find_program(BRLCAD_CONFIGEXE brlcad-config)
  if(BRLCAD_CONFIGEXE)
    execute_process(COMMAND brlcad-config --prefix OUTPUT_VARIABLE BRLCAD_ROOT)
  endif()

  # If that didn't work, see if we can find brlcad-config and use the parent path
  if(NOT BRLCAD_ROOT)
    find_path(BRLCAD_BIN_DIR brlcad-config)
    if(BRLCAD_BIN_DIR)
      get_filename_component(BRLCAD_ROOT ${BRLCAD_BIN_DIR} PATH)
    endif()
  endif()

  # Look for headers if we come up empty with brlcad-config
  if(NOT BRLCAD_ROOT)
    file(GLOB CAD_DIRS LIST_DIRECTORIES TRUE "/usr/brlcad/*")
    foreach(CDIR ${CAD_DIRS})
      if(IS_DIRECTORY ${CDIR})
        list(APPEND BRLCAD_HEADERS_DIR_CANDIDATES "${CDIR}/include/brlcad")
      endif()
    endforeach()
    file(GLOB CAD_DIRS_LOCAL LIST_DIRECTORIES TRUE "/usr/local/brlcad/*")
    foreach(CDIR ${CAD_DIRS_LOCAL})
      if(IS_DIRECTORY ${CDIR})
        list(APPEND BRLCAD_HEADERS_DIR_CANDIDATES "${CDIR}/include/brlcad")
      endif()
    endforeach()

    # If there are multiple installed BRL-CAD versions present,
    # we want the newest
    if(${CMAKE_VERSION} VERSION_GREATER "3.17.99")
      list(SORT BRLCAD_HEADERS_DIR_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    endif()

    # Look for brlcad.h
    foreach(CDIR ${BRLCAD_HEADERS_DIR_CANDIDATES})
      if(NOT BRLCAD_ROOT)
        find_path(BRLCAD_HEADERS_DIR NAMES brlcad.h PATHS ${CDIR})
        if(BRLCAD_HEADERS_DIR)
          get_filename_component(BRLCAD_INC_DIR ${BRLCAD_HEADERS_DIR} PATH)
          get_filename_component(BRLCAD_ROOT ${BRLCAD_INC_DIR} PATH)
        endif()
      endif()
    endforeach()
  endif()
endif()

#Find include directories
if(NOT BRLCAD_HEADERS_DIR)
  find_path(
    BRLCAD_HEADERS_DIR
    NAMES brlcad.h
    HINTS ${BRLCAD_ROOT}
    PATH_SUFFIXES include/brlcad
  )
  get_filename_component(BRLCAD_HEADERS_PARENT_DIR ${BRLCAD_HEADERS_DIR} PATH)
endif()
find_path(
  BRLCAD_OPENNURBS_HEADERS_DIR
  NAMES opennurbs.h
  HINTS ${BRLCAD_ROOT}
  PATH_SUFFIXES include/openNURBS include/opennurbs
)
set(
  BRLCAD_INCLUDE_DIR
  ${BRLCAD_HEADERS_PARENT_DIR}
  ${BRLCAD_HEADERS_DIR}
  ${BRLCAD_OPENNURBS_HEADERS_DIR}
)

#Find library directory
if(WIN32)
  # Windows is a rather odd special case - it's shared library suffix is "dll",
  # but what we want for this purpose is the "lib" files
  find_path(
    BRLCAD_LIB_DIR
    "libbu.lib"
    PATHS ${BRLCAD_ROOT}
    PATH_SUFFIXES lib libs
  )
else(WIN32)
  find_path(
    BRLCAD_LIB_DIR
    "libbu${CMAKE_SHARED_LIBRARY_SUFFIX}"
    PATHS ${BRLCAD_ROOT}
    PATH_SUFFIXES lib libs
  )
endif(WIN32)

#Find binary directory
if(NOT BRLCAD_BIN_DIR)
  find_path(BRLCAD_BIN_DIR brlcad-config PATHS ${BRLCAD_ROOT} PATH_SUFFIXES bin)
endif(NOT BRLCAD_BIN_DIR)

#Attempt to get brlcad version.
if(NOT BRLCAD_CONFIGEXE)
  find_program(
    BRLCAD_CONFIGEXE
    brlcad-config
    PATHS ${BRLCAD_ROOT}
    PATH_SUFFIXES bin
  )
endif(NOT BRLCAD_CONFIGEXE)
if(BRLCAD_CONFIGEXE)
  execute_process(
    COMMAND ${BRLCAD_CONFIGEXE} --version
    OUTPUT_VARIABLE BRLCAD_VERSION
  )
  # Don't try to strip if BRLCAD_VERSION is empty - CMake doesn't like it and will
  # return an error.
  if(NOT "${BRLCAD_VERSION}" STREQUAL "")
    string(STRIP ${BRLCAD_VERSION} BRLCAD_VERSION)
    if(BRLCAD_VERSION)
      string(
        REGEX REPLACE
        "([0-9]+)\\.[0-9]+\\.[0-9]+"
        "\\1"
        BRLCAD_VERSION_MAJOR
        "${BRLCAD_VERSION}"
      )
      string(
        REGEX REPLACE
        "[0-9]+\\.([0-9]+)\\.[0-9]+"
        "\\1"
        BRLCAD_VERSION_MINOR
        "${BRLCAD_VERSION}"
      )
      string(
        REGEX REPLACE
        "[0-9]+\\.[0-9]+\\.([0-9]+)"
        "\\1"
        BRLCAD_VERSION_PATCH
        "${BRLCAD_VERSION}"
      )
    endif(BRLCAD_VERSION)
  endif(NOT "${BRLCAD_VERSION}" STREQUAL "")
endif(BRLCAD_CONFIGEXE)
if(NOT BRLCAD_VERSION AND BRLCAD_HEADERS_DIR)
  # If we don't have a working brlcad-config, we can try reading the info
  # directly from brlcad_version.h
  if(EXISTS "${BRLCAD_HEADERS_DIR}/brlcad_version.h")
    file(STRINGS "${BRLCAD_HEADERS_DIR}/brlcad_version.h" VINFO)
    foreach(vline ${VINFO})
      if("${vline}" MATCHES "define BRLCAD_VERSION_MAJOR [0-9]+")
        string(
          REGEX REPLACE
          ".*define BRLCAD_VERSION_MAJOR "
          ""
          BRLCAD_VERSION_MAJOR
          ${vline}
        )
        string(STRIP ${BRLCAD_VERSION_MAJOR} BRLCAD_VERSION_MAJOR)
      endif("${vline}" MATCHES "define BRLCAD_VERSION_MAJOR [0-9]+")
      if("${vline}" MATCHES "define BRLCAD_VERSION_MINOR [0-9]+")
        string(
          REGEX REPLACE
          ".*define BRLCAD_VERSION_MINOR "
          ""
          BRLCAD_VERSION_MINOR
          ${vline}
        )
        string(STRIP ${BRLCAD_VERSION_MINOR} BRLCAD_VERSION_MINOR)
      endif("${vline}" MATCHES "define BRLCAD_VERSION_MINOR [0-9]+")
      if("${vline}" MATCHES "define BRLCAD_VERSION_PATCH [0-9]+")
        string(
          REGEX REPLACE
          ".*define BRLCAD_VERSION_PATCH "
          ""
          BRLCAD_VERSION_PATCH
          ${vline}
        )
        string(STRIP ${BRLCAD_VERSION_PATCH} BRLCAD_VERSION_PATCH)
      endif("${vline}" MATCHES "define BRLCAD_VERSION_PATCH [0-9]+")
    endforeach(vline ${VINFO})
    set(
      BRLCAD_VERSION
      "${BRLCAD_VERSION_MAJOR}.${BRLCAD_VERSION_MINOR}.${BRLCAD_VERSION_PATCH}"
    )
  endif(EXISTS "${BRLCAD_HEADERS_DIR}/brlcad_version.h")
endif(NOT BRLCAD_VERSION AND BRLCAD_HEADERS_DIR)
if(NOT BRLCAD_VERSION)
  set(BRLCAD_VERSION_FOUND FALSE)
else(NOT BRLCAD_VERSION)
  set(
    BRLCAD_VERSION_MAJOR
    ${BRLCAD_VERSION_MAJOR}
    CACHE STRING
    "BRL-CAD major version"
  )
  set(
    BRLCAD_VERSION_MINOR
    ${BRLCAD_VERSION_MINOR}
    CACHE STRING
    "BRL-CAD minor version"
  )
  set(
    BRLCAD_VERSION_PATCH
    ${BRLCAD_VERSION_PATCH}
    CACHE STRING
    "BRL-CAD patch version"
  )
  set(BRLCAD_VERSION ${BRLCAD_VERSION} CACHE STRING "BRL-CAD version")
  set(BRLCAD_VERSION_FOUND TRUE)
endif(NOT BRLCAD_VERSION)

##########################################################################
# Search for BRL-CAD's own libraries
set(
  BRLCAD_REQ_LIB_NAMES
  analyze
  bg
  bn
  bv
  brep
  bu
  dm
  fft
  gcv
  ged
  icv
  nmg
  optical
  pkg
  render
  rt
  wdb
)

set(BRLCAD_REQ_LIBS)
foreach(brl_lib ${BRLCAD_REQ_LIB_NAMES})
  string(TOUPPER ${brl_lib} LIBCORE)
  find_library(
    BRLCAD_${LIBCORE}_LIBRARY
    NAMES ${brl_lib} lib${brl_lib}
    PATHS ${BRLCAD_LIB_DIR} NO_SYSTEM_PATH
  )
  set(BRLCAD_REQ_LIBS ${BRLCAD_REQ_LIBS} BRLCAD_${LIBCORE}_LIBRARY)
  if(BRLCAD_${LIBCORE}_LIBRARY)
    set(BRLCAD_LIBRARIES ${BRLCAD_LIBRARIES} ${BRLCAD_${LIBCORE}_LIBRARY})
  else(BRLCAD_${LIBCORE}_LIBRARY)
    set(BRLCAD_LIBRARIES_NOTFOUND ${BRLCAD_LIBRARIES_NOTFOUND} ${brl_lib})
  endif(BRLCAD_${LIBCORE}_LIBRARY)
endforeach(brl_lib ${BRLCAD_REQ_LIB_NAMES})

# Optional libraries - we will still return a successful
# search if these aren't here
set(BRLCAD_OPT_LIB_NAMES tclcad qtcad)
foreach(brl_lib ${BRLCAD_OPT_LIB_NAMES})
  string(TOUPPER ${brl_lib} LIBCORE)
  find_library(
    BRLCAD_${LIBCORE}_LIBRARY
    NAMES ${brl_lib} lib${brl_lib}
    PATHS ${BRLCAD_LIB_DIR} NO_SYSTEM_PATH
  )
  if(BRLCAD_${LIBCORE}_LIBRARY)
    set(BRLCAD_LIBRARIES ${BRLCAD_LIBRARIES} ${BRLCAD_${LIBCORE}_LIBRARY})
  endif(BRLCAD_${LIBCORE}_LIBRARY)
endforeach(brl_lib ${BRLCAD_OPT_LIB_NAMES})

# Then, look for customized src/other libraries that we need
# local versions of

set(BRL-CAD_SRC_OTHER_REQUIRED openNURBS)

foreach(ext_lib ${BRL-CAD_LIBS_SEARCH_LIST})
  string(TOUPPER ${ext_lib} LIBCORE)
  find_library(
    BRLCAD_${LIBCORE}_LIBRARY
    NAMES ${ext_lib} lib${ext_lib}
    PATHS ${BRLCAD_LIB_DIR} NO_SYSTEM_PATH
  )
  set(BRLCAD_REQ_LIBS ${BRLCAD_REQ_LIBS} BRLCAD_${LIBCORE}_LIBRARY)
  if(BRLCAD_${LIBCORE}_LIBRARY)
    set(BRLCAD_LIBRARIES ${BRLCAD_LIBRARIES} ${BRLCAD_${LIBCORE}_LIBRARY})
  else(BRLCAD_${LIBCORE}_LIBRARY)
    set(BRLCAD_LIBRARIES_NOTFOUND ${BRLCAD_LIBRARIES_NOTFOUND} ${ext_lib})
  endif(BRLCAD_${LIBCORE}_LIBRARY)
endforeach(ext_lib ${BRL-CAD_LIBS_SEARCH_LIST})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  BRLCAD
  FOUND_VAR BRLCAD_FOUND
  REQUIRED_VARS ${BRLCAD_REQ_LIBS} BRLCAD_INCLUDE_DIR
  VERSION_VAR BRLCAD_VERSION
  FAIL_MESSAGE
    "\nBRL-CAD not found.  Try setting BRLCAD_ROOT to the directory containing the bin, include, and lib dirs (for example, -DBRLCAD_ROOT=/usr/brlcad/rel-7.32.4)\n"
)

if(BRLCAD_FOUND)
  set(
    BRLCAD_INCLUDE_DIRS
    ${BRLCAD_INCLUDE_DIR}
    CACHE STRING
    "BRL-CAD include directories"
  )
  set(BRLCAD_LIBRARIES ${BRLCAD_LIBRARIES} CACHE STRING "BRL-CAD libraries")
  # Set up a "modern" CMake target to make it easer for
  # client codes to reference the BRL-CAD libs.  See
  # https://stackoverflow.com/a/48397346/2037687
  set(libtargets)
  foreach(brl_lib ${BRLCAD_REQ_LIB_NAMES})
    string(TOUPPER ${brl_lib} LIBCORE)
    if(NOT TARGET BRLCAD::${brl_lib})
      add_library(BRLCAD::${brl_lib} UNKNOWN IMPORTED)
      set_target_properties(
        BRLCAD::${brl_lib}
        PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${BRLCAD_INCLUDE_DIR}"
          IMPORTED_LOCATION ${BRLCAD_${LIBCORE}_LIBRARY}
          IMPORTED_LOCATION_DEBUG ${BRLCAD_${LIBCORE}_LIBRARY}
      )
    endif()
    # Also set the legacy uppercase variable for compatibility
    set(BRLCAD_${LIBCORE}_LIBRARY BRLCAD::${brl_lib})
    set(libtargets ${libtargets} BRLCAD::${brl_lib})
  endforeach(brl_lib ${BRLCAD_REQ_LIB_NAMES})
  # For the optional libs, add them IFF they are present
  foreach(brl_lib ${BRLCAD_OPT_LIB_NAMES})
    string(TOUPPER ${brl_lib} LIBCORE)
    if(BRLCAD_${LIBCORE}_LIBRARY)
      if(NOT TARGET BRLCAD::${brl_lib})
        add_library(BRLCAD::${brl_lib} UNKNOWN IMPORTED)
        set_target_properties(
          BRLCAD::${brl_lib}
          PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${BRLCAD_INCLUDE_DIR}"
            IMPORTED_LOCATION ${BRLCAD_${LIBCORE}_LIBRARY}
            IMPORTED_LOCATION_DEBUG ${BRLCAD_${LIBCORE}_LIBRARY}
        )
      endif()
      set(BRLCAD_${LIBCORE}_LIBRARY BRLCAD::${brl_lib})
      set(libtargets ${libtargets} BRLCAD::${brl_lib})
    endif(BRLCAD_${LIBCORE}_LIBRARY)
  endforeach(brl_lib ${BRLCAD_OPT_LIB_NAMES})
  # Overall "kitchen sink" target
  if(NOT TARGET BRLCAD::brlcad)
    add_library(BRLCAD::brlcad INTERFACE IMPORTED)
    set_property(
      TARGET BRLCAD::brlcad
      PROPERTY INTERFACE_LINK_LIBRARIES ${libtargets}
    )
  endif()
  set(BRLCAD_LIBRARIES BRLCAD::brlcad)
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
