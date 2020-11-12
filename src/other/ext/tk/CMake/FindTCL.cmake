# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindTCL
-------

This module finds if Tcl is installed and determines where the include
files and libraries are.  It also determines what the name of the
library is.  This code sets the following variables:

::

TCL_FOUND              = Tcl was found
TCLTK_FOUND            = Tcl and Tk were found
TCLSH_FOUND            = TRUE if tclsh has been found
TCL_LIBRARY            = path to Tcl library (tcl tcl80)
TCL_INCLUDE_PATH       = path to where tcl.h can be found
TCL_TCLSH              = path to tclsh binary (tcl tcl80)
TCL_STUB_LIBRARY       = path to Tcl stub library

#]=======================================================================]

include(CMakeFindFrameworks)

set(_TCL_SEARCHES)

# Search TCL_ROOT first if it is set.
if(TCL_ROOT)
  set(_TCL_SEARCH_ROOT PATHS ${TCL_ROOT} NO_DEFAULT_PATH)
  list(APPEND _TCL_SEARCHES _TCL_SEARCH_ROOT)
endif()

if(WIN32)
  get_filename_component(
    ActiveTcl_CurrentVersion
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl;CurrentVersion]"
    NAME)
  set(TCLTK_POSSIBLE_BIN_PATHS ${TCLTK_POSSIBLE_BIN_PATHS}
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.6;Root]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.5;Root]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.4;Root]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.3;Root]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.2;Root]/bin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.0;Root]/bin"
    )

  get_filename_component(
    ActiveTcl_CurrentVersion
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl;CurrentVersion]"
    NAME)
  set(TCLTK_POSSIBLE_LIB_PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.6;Root]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.5;Root]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.4;Root]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.3;Root]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.2;Root]/lib"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.0;Root]/lib"
    "$ENV{ProgramFiles}/Tcl/Lib"
    "C:/Program Files/Tcl/lib"
    "C:/Tcl/lib"
    )

  set(TCLTK_POSSIBLE_INCLUDE_PATHS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ActiveState\\ActiveTcl\\${ActiveTcl_CurrentVersion}]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.6;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.5;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.4;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.3;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.2;Root]/include"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Scriptics\\Tcl\\8.0;Root]/include"
    "$ENV{ProgramFiles}/Tcl/include"
    "C:/Program Files/Tcl/include"
    "C:/Tcl/include"
    )
endif()

set(TCL_TCLSH_NAMES
  tclsh
  tclsh${TCL_LIBRARY_VERSION} tclsh${TK_LIBRARY_VERSION}
  tclsh87 tclsh8.7
  tclsh86 tclsh8.6
  tclsh85 tclsh8.5
  tclsh84 tclsh8.4
  tclsh83 tclsh8.3
  tclsh82 tclsh8.2
  tclsh80 tclsh8.0
  )
if(CYGWIN)
  set(TCL_TCLSH_NAMES ${TCL_TCLSH_NAMES} cygtclsh83 cygtclsh80)
endif(CYGWIN)

foreach(search ${_TCL_SEARCHES})
  find_program(TCL_TCLSH NAMES ${TCL_TCLSH_NAMES} ${${search}} PATH_SUFFIXES bin)
endforeach()
if (NOT TCL_TCLSH)
  find_program(TCL_TCLSH NAMES ${TCL_TCLSH_NAMES} HINTS ${TCLTK_POSSIBLE_BIN_PATHS})
endif (NOT TCL_TCLSH)

if(TCLSH_VERSION_STRING)
  set(TCL_TCLSH_VERSION "${TCLSH_VERSION_STRING}")
else()
  get_filename_component(TCL_TCLSH_PATH "${TCL_TCLSH}" PATH)
  get_filename_component(TCL_TCLSH_PATH_PARENT "${TCL_TCLSH_PATH}" PATH)
  string(REGEX REPLACE
    "^.*tclsh([0-9]\\.*[0-9]).*$" "\\1" TCL_TCLSH_VERSION "${TCL_TCLSH}")
endif()

set(TCLTK_POSSIBLE_LIB_PATHS
  "${TCL_INCLUDE_PATH_PARENT}/lib"
  "${TK_INCLUDE_PATH_PARENT}/lib"
  "${TCL_LIBRARY_PATH}"
  "${TK_LIBRARY_PATH}"
  "${TCL_TCLSH_PATH_PARENT}/lib"
  )
set(TCLTK_POSSIBLE_LIB_PATH_SUFFIXES
  lib
  lib/tcl
  lib/tcl/tcl8.7
  lib/tcl/tk8.7
  lib/tcl/tcl8.6
  lib/tcl/tk8.6
  lib/tcl/tcl8.5
  lib/tcl/tk8.5
  lib/tcl/tcl8.4
  lib/tcl/tk8.4
  )

set(TCL_POSSIBLE_LIB_NAMES
  tcl
  tcl${TCL_LIBRARY_VERSION} tcl${TCL_TCLSH_VERSION}
  tcl87 tcl8.7 tcl87t tcl8.7t
  tcl86 tcl8.6 tcl86t tcl8.6t
  tcl85 tcl8.5
  tcl84 tcl8.4
  tcl83 tcl8.3
  tcl82 tcl8.2
  tcl80 tcl8.0
  )

if(NOT TCL_LIBRARY)
  foreach(search ${_TCL_SEARCHES})
    find_library(TCL_LIBRARY
      NAMES ${TCL_POSSIBLE_LIB_NAMES}
      NAMES_PER_DIR ${${search}}
      PATH_SUFFIXES ${TCLTK_POSSIBLE_LIB_PATH_SUFFIXES})
  endforeach()
endif()
if(NOT TCL_LIBRARY)
  find_library(TCL_LIBRARY
    NAMES ${TCL_POSSIBLE_LIB_NAMES}
    PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
    PATH_SUFFIXES ${TCLTK_POSSIBLE_LIB_PATH_SUFFIXES}
    )
endif(NOT TCL_LIBRARY)

set(TCLSTUB_POSSIBLE_LIB_NAMES
  tclstub
  tclstub${TK_LIBRARY_VERSION} tclstub${TCL_TCLSH_VERSION}
  tclstub87 tclstub8.7
  tclstub86 tclstub8.6
  tclstub85 tclstub8.5
  tclstub84 tclstub8.4
  tclstub83 tclstub8.3
  tclstub82 tclstub8.2
  tclstub80 tclstub8.0
  )
if(NOT TCL_STUB_LIBRARY)
  foreach(search ${_TCL_SEARCHES})
    find_library(TCL_STUB_LIBRARY
      NAMES ${TCLSTUB_POSSIBLE_LIB_NAMES}
      NAMES_PER_DIR ${${search}}
      PATH_SUFFIXES ${TCLTK_POSSIBLE_LIB_PATH_SUFFIXES}
      )
  endforeach()
endif()
if(NOT TCL_STUB_LIBRARY)
  find_library(TCL_STUB_LIBRARY
    NAMES ${TCLSTUB_POSSIBLE_LIB_NAMES}
    PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
    )
endif()

CMAKE_FIND_FRAMEWORKS(Tcl)

set(TCL_FRAMEWORK_INCLUDES)
if(Tcl_FRAMEWORKS)
  if(NOT TCL_INCLUDE_PATH)
    foreach(dir ${Tcl_FRAMEWORKS})
      set(TCL_FRAMEWORK_INCLUDES ${TCL_FRAMEWORK_INCLUDES} ${dir}/Headers)
    endforeach()
  endif()
endif()

set(TCLTK_POSSIBLE_INCLUDE_PATHS
  "${TCL_LIBRARY_PATH_PARENT}/include"
  "${TCL_INCLUDE_PATH}"
  ${TCL_FRAMEWORK_INCLUDES}
  "${TCL_TCLSH_PATH_PARENT}/include"
  )

set(TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES
  include
  include/tcl${TCL_LIBRARY_VERSION}
  include/tcl8.7
  include/tcl8.6
  include/tcl8.5
  include/tcl8.4
  include/tcl8.3
  include/tcl8.2
  include/tcl8.0
  )

foreach(search ${_TCL_SEARCHES})
  find_path(TCL_INCLUDE_PATH
    NAMES tcl.h ${${search}}
    PATH_SUFFIXES ${TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES}
    )
endforeach()
if (NOT TCL_INCLUDE_PATH)
  find_path(TCL_INCLUDE_PATH
    NAMES tcl.h
    HINTS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
    PATH_SUFFIXES ${TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES}
    )
endif()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL
  REQUIRED_VARS TCL_LIBRARY TCL_STUB_LIBRARY TCL_INCLUDE_PATH TCL_TCLSH
  VERSION_VAR TCLSH_VERSION_STRING)
set(FPHSA_NAME_MISMATCHED 1)
set(TCLTK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
set(TCLTK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCLTK
  REQUIRED_VARS TCL_LIBRARY TCL_STUB_LIBRARY TCL_INCLUDE_PATH)
unset(FPHSA_NAME_MISMATCHED)

mark_as_advanced(
  TCL_INCLUDE_PATH
  TCL_LIBRARY
  TCL_STUB_LIBRARY
  TCL_TCLSH
  )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
