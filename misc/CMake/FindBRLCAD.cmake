#                F I N D B R L C A D . C M A K E
# BRL-CAD
#
# Copyright (c) 2024-2025 United States Government as represented by
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
#  Try to find brlcad libraries.
#  Once done, this will define:
#
#   BRLCAD_FOUND - system has BRL-CAD
#   BRLCAD_VERSION - the BRL-CAD version string
#   BRLCAD_INCLUDE_DIRS - the BRL-CAD include directories
#   BRLCAD_LIBRARIES - link these to use the BRL-CAD Libraries
#
#   BRLCAD_ANALYZE_LIBRARY - BRL-CAD Analysis library
#   BRLCAD_BG_LIBRARY - BRL-CAD Geometry Algorithms library
#   BRLCAD_BN_LIBRARY - BRL-CAD Numerical library
#   BRLCAD_BV_LIBRARY - BRL-CAD View management library
#   BRLCAD_BREP_LIBRARY - BRL-CAD NURBS Brep Algorithms library
#   BRLCAD_BU_LIBRARY - BRL-CAD Utility library
#   BRLCAD_DM_LIBRARY - BRL-CAD Display Manager/Framebuffer library
#   BRLCAD_FFT_LIBRARY - BRL-CAD FFT library
#   BRLCAD_GCV_LIBRARY - BRL-CAD Geometry Conversion library
#   BRLCAD_GED_LIBRARY - BRL-CAD Geometry Editing library
#   BRLCAD_ICV_LIBRARY - BRL-CAD Image Conversion library
#   BRLCAD_NMG_LIBRARY - BRL-CAD N-Manifold Generation library
#   BRLCAD_OPTICAL_LIBRARY - BRL-CAD optical library
#   BRLCAD_PKG_LIBRARY - BRL-CAD libpkg
#   BRLCAD_QTCAD_LIBRARY - BRL-CAD libqtcad (currently optional)
#   BRLCAD_RENDER_LIBRARY - librender
#   BRLCAD_RT_LIBRARY - BRL-CAD Raytracing library
#   BRLCAD_TCLCAD_LIBRARY - libtclcad
#   BRLCAD_WDB_LIBRARY - BRL-CAD Write Database library
#
#   NOTE: customized version needed for BRL-CAD NURBS support
#   BRLCAD_OPENNURBS_LIBRARY - OpenNURBS library
#
#########################################################################

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
    add_library(BRLCAD::${LIBCORE} UNKNOWN IMPORTED)
    set_target_properties(
      BRLCAD::${LIBCORE}
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${BRLCAD_INCLUDE_DIR}"
        IMPORTED_LOCATION ${BRLCAD_${LIBCORE}_LIBRARY}
        IMPORTED_LOCATION_DEBUG ${BRLCAD_${LIBCORE}_LIBRARY}
    )
    set(libtargets ${libtargets} BRLCAD::${LIBCORE})
  endforeach(brl_lib ${BRL-CAD_LIBS_SEARCH_LIST})
  # For the optional libs, add them IFF they are present
  foreach(brl_lib ${BRLCAD_OPT_LIB_NAMES})
    string(TOUPPER ${brl_lib} LIBCORE)
    if(BRLCAD_${LIBCORE}_LIBRARY)
      add_library(BRLCAD::${LIBCORE} UNKNOWN IMPORTED)
      set_target_properties(
        BRLCAD::${LIBCORE}
        PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${BRLCAD_INCLUDE_DIR}"
          IMPORTED_LOCATION ${BRLCAD_${LIBCORE}_LIBRARY}
          IMPORTED_LOCATION_DEBUG ${BRLCAD_${LIBCORE}_LIBRARY}
      )
      set(libtargets ${libtargets} BRLCAD::${LIBCORE})
    endif(BRLCAD_${LIBCORE}_LIBRARY)
  endforeach(brl_lib ${BRLCAD_OPT_LIB_NAMES})
  # Overall "kitchen sink" target
  add_library(BRLCAD::BRLCAD INTERFACE IMPORTED)
  set_property(
    TARGET BRLCAD::BRLCAD
    PROPERTY INTERFACE_LINK_LIBRARIES ${libtargets}
  )
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

