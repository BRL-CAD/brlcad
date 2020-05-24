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
  TK_FOUND               = Tk was found
  TCLTK_FOUND            = Tcl and Tk were found
  TCLSH_FOUND            = TRUE if tclsh has been found
  TCL_LIBRARY            = path to Tcl library (tcl tcl80)
  TCL_INCLUDE_PATH       = path to where tcl.h can be found
  TCL_TCLSH              = path to tclsh binary (tcl tcl80)
  TK_LIBRARY             = path to Tk library (tk tk80 etc)
  TK_INCLUDE_PATH        = path to where tk.h can be found
  TK_WISH                = full path to the wish executable
  TCL_STUB_LIBRARY       = path to Tcl stub library
  TK_STUB_LIBRARY        = path to Tk stub library
  TTK_STUB_LIBRARY       = path to ttk stub library

#]=======================================================================]

include(CMakeFindFrameworks)

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
  tclsh${TCL_LIBRARY_VERSION} tclsh${TK_LIBRARY_VERSION} tclsh${TK_WISH_VERSION}
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


find_program(TCL_TCLSH
  NAMES ${TCL_TCLSH_NAMES}
  HINTS ${TCLTK_POSSIBLE_BIN_PATHS}
  )

set(TK_WISH_NAMES
  wish
  wish${TCL_LIBRARY_VERSION} wish${TK_LIBRARY_VERSION} wish${TCL_TCLSH_VERSION}
  wish86 wish8.6
  wish85 wish8.5
  wish84 wish8.4
  wish83 wish8.3
  wish82 wish8.2
  wish80 wish8.0
  )

if(CYGWIN)
	set(TK_WISH_NAMES ${TK_WISH_NAMES} cygwish80 )
endif()

find_program(TK_WISH
  NAMES ${TK_WISH_NAMES}
  HINTS ${TCLTK_POSSIBLE_BIN_PATHS}
  )

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
  "${TK_WISH_PATH_PARENT}/lib"
)

set(TCLTK_POSSIBLE_LIB_PATH_SUFFIXES
  lib/tcl/tcl8.7
  lib/tcl/tk8.7
  lib/tcl/tcl8.6
  lib/tcl/tk8.6
  lib/tcl/tcl8.5
  lib/tcl/tk8.5
  lib/tcl/tcl8.4
  lib/tcl/tk8.4
)

find_library(TCL_LIBRARY
  NAMES
  tcl
  tcl${TCL_LIBRARY_VERSION} tcl${TCL_TCLSH_VERSION} tcl${TK_WISH_VERSION}
  tcl87 tcl8.7 tcl87t tcl8.7t
  tcl86 tcl8.6 tcl86t tcl8.6t
  tcl85 tcl8.5
  tcl84 tcl8.4
  tcl83 tcl8.3
  tcl82 tcl8.2
  tcl80 tcl8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
  PATH_SUFFIXES ${TCLTK_POSSIBLE_LIB_PATH_SUFFIXES}
  )

find_library(TCL_STUB_LIBRARY
  NAMES
  tclstub
  tclstub${TK_LIBRARY_VERSION} tclstub${TCL_TCLSH_VERSION} tclstub${TK_WISH_VERSION}
  tclstub87 tclstub8.7
  tclstub86 tclstub8.6
  tclstub85 tclstub8.5
  tclstub84 tclstub8.4
  tclstub83 tclstub8.3
  tclstub82 tclstub8.2
  tclstub80 tclstub8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
)

find_library(TK_LIBRARY
  NAMES
  tk
  tk${TK_LIBRARY_VERSION} tk${TCL_TCLSH_VERSION} tk${TK_WISH_VERSION}
  tk87 tk8.7 tk87t tk8.7t
  tk86 tk8.6 tk86t tk8.6t
  tk85 tk8.5
  tk84 tk8.4
  tk83 tk8.3
  tk82 tk8.2
  tk80 tk8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
  PATH_SUFFIXES ${TCLTK_POSSIBLE_LIB_PATH_SUFFIXES}
  )

find_library(TK_STUB_LIBRARY
  NAMES
  tkstub
  tkstub${TCL_LIBRARY_VERSION} tkstub${TCL_TCLSH_VERSION} tkstub${TK_WISH_VERSION}
  tkstub87 tkstub8.7
  tkstub86 tkstub8.6
  tkstub85 tkstub8.5
  tkstub84 tkstub8.4
  tkstub83 tkstub8.3
  tkstub82 tkstub8.2
  tkstub80 tkstub8.0
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
)

find_library(TTK_STUB_LIBRARY
  NAMES
  ttkstub
  ttkstub${TCL_LIBRARY_VERSION} ttkstub${TCL_TCLSH_VERSION} ttkstub${TK_WISH_VERSION}
  ttkstub88 ttkstub8.8
  ttkstub87 ttkstub8.7
  ttkstub86 ttkstub8.6
  ttkstub85 ttkstub8.5
  PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
)

CMAKE_FIND_FRAMEWORKS(Tcl)
CMAKE_FIND_FRAMEWORKS(Tk)

set(TCL_FRAMEWORK_INCLUDES)
if(Tcl_FRAMEWORKS)
  if(NOT TCL_INCLUDE_PATH)
    foreach(dir ${Tcl_FRAMEWORKS})
      set(TCL_FRAMEWORK_INCLUDES ${TCL_FRAMEWORK_INCLUDES} ${dir}/Headers)
    endforeach()
  endif()
endif()

set(TK_FRAMEWORK_INCLUDES)
if(Tk_FRAMEWORKS)
  if(NOT TK_INCLUDE_PATH)
    foreach(dir ${Tk_FRAMEWORKS})
      set(TK_FRAMEWORK_INCLUDES ${TK_FRAMEWORK_INCLUDES}
        ${dir}/Headers ${dir}/PrivateHeaders)
    endforeach()
  endif()
endif()

set(TCLTK_POSSIBLE_INCLUDE_PATHS
  "${TCL_LIBRARY_PATH_PARENT}/include"
  "${TK_LIBRARY_PATH_PARENT}/include"
  "${TCL_INCLUDE_PATH}"
  "${TK_INCLUDE_PATH}"
  ${TCL_FRAMEWORK_INCLUDES}
  ${TK_FRAMEWORK_INCLUDES}
  "${TCL_TCLSH_PATH_PARENT}/include"
  "${TK_WISH_PATH_PARENT}/include"
  )

set(TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES
  include/tcl${TK_LIBRARY_VERSION}
  include/tcl${TCL_LIBRARY_VERSION}
  include/tcl8.7
  include/tk8.7
  include/tcl8.6
  include/tk8.6
  include/tcl8.5
  include/tk8.5
  include/tcl8.4
  include/tk8.4
  include/tcl8.3
  include/tcl8.2
  include/tcl8.0
  )

find_path(TCL_INCLUDE_PATH
  NAMES tcl.h
  HINTS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
  PATH_SUFFIXES ${TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES}
  )

find_path(TK_INCLUDE_PATH
  NAMES tk.h
  HINTS ${TCLTK_POSSIBLE_INCLUDE_PATHS}
  PATH_SUFFIXES ${TCLTK_POSSIBLE_INCLUDE_PATH_SUFFIXES}
  )

# IFF we have TCL_TK_SYSTEM_GRAPHICS set and have a system TK_WISH, check that the
# windowing system matches the specified type
if (NOT "${TCL_TK_SYSTEM_GRAPHICS}" STREQUAL "" AND TK_WISH AND NOT TARGET "${TK_WISH}")
	set(tkwin_script "
	set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM\"
	set fileId [open $filename \"w\"]
	set windowingsystem [tk windowingsystem]
	puts $fileId $windowingsystem
	close $fileId
	exit
	")
	set(tkwin_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tk_windowingsystem.tcl")
	set(WSYS "wm-NOTFOUND")
	file(WRITE ${tkwin_scriptfile} ${tkwin_script})
	execute_process(COMMAND ${TK_WISH} ${tkwin_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
	if (EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM")
		file(READ "${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM" readresultvar)
		string(REGEX REPLACE "\n" "" WSYS "${readresultvar}")
	endif (EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/TK_WINDOWINGSYSTEM")

	# If we have no information about the windowing system or it does not match
	# a specified system, the find_package detection has failed
	if (NOT "${WSYS}" STREQUAL "${TCL_TK_SYSTEM_GRAPHICS}")
		unset(TCL_LIBRARY CACHE)
		unset(TCL_STUB_LIBRARY CACHE)
		unset(TK_LIBRARY CACHE)
		unset(TK_STUB_LIBRARY CACHE)
		unset(TCL_FOUND CACHE)
		unset(TK_FOUND CACHE)
		unset(TCLTK_FOUND CACHE)
		unset(TCLSH_FOUND CACHE)
		unset(TCL_LIBRARY CACHE)
		unset(TCL_INCLUDE_PATH CACHE)
		unset(TCL_TCLSH CACHE)
		unset(TK_LIBRARY CACHE)
		unset(TK_INCLUDE_PATH CACHE)
		unset(TK_WISH CACHE)
		unset(TCL_STUB_LIBRARY CACHE)
		unset(TK_STUB_LIBRARY CACHE)
		unset(TTK_STUB_LIBRARY CACHE)
	endif (NOT "${WSYS}" STREQUAL "${TCL_TK_SYSTEM_GRAPHICS}")
endif (NOT "${TCL_TK_SYSTEM_GRAPHICS}" STREQUAL "" AND TK_WISH AND NOT TARGET "${TK_WISH}")

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCL
	REQUIRED_VARS TCL_LIBRARY TCL_STUB_LIBRARY TCL_INCLUDE_PATH TCL_TCLSH
	VERSION_VAR TCLSH_VERSION_STRING)
set(FPHSA_NAME_MISMATCHED 1)
set(TCLTK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
set(TCLTK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TCLTK
	REQUIRED_VARS TCL_LIBRARY TCL_STUB_LIBRARY TCL_INCLUDE_PATH TK_LIBRARY TK_STUB_LIBRARY TK_INCLUDE_PATH)
set(TK_FIND_REQUIRED ${TCL_FIND_REQUIRED})
set(TK_FIND_QUIETLY  ${TCL_FIND_QUIETLY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TK
	REQUIRED_VARS TK_LIBRARY TK_STUB_LIBRARY TK_INCLUDE_PATH TK_WISH)
unset(FPHSA_NAME_MISMATCHED)

mark_as_advanced(
  TCL_INCLUDE_PATH
  TCL_LIBRARY
  TCL_STUB_LIBRARY
  TCL_TCLSH
  TK_INCLUDE_PATH
  TK_LIBRARY
  TK_STUB_LIBRARY
  TK_WISH
  TTK_STUB_LIBRARY
  )

