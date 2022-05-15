#                     C M A K E L I S T S . T X T
# ITCL
#
# Copyright (c) 2010 United States Government as represented by
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

# *******************************************************************
# ***                   ITCL CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(ITCL)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

find_package(TCL)

# version numbers
set(ITCL_VERSION_MAJOR 3)
set(ITCL_VERSION_MINOR 4)
set(ITCL_PATCH_LEVEL 3)

set(ITCL_VERSION "${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR}")

#---------------------------------------------------------------------
# Initialize some relative variable names
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT SHARED_DIR)
  if(NOT WIN32)
    set(SHARED_DIR lib)
  else(NOT WIN32)
    set(SHARED_DIR bin)
  endif(NOT WIN32)
endif(NOT SHARED_DIR)
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

#----------------------------------------------------------------------------
# First, get some standard options out of the way
add_definitions(-DSTDC_HEADERS=1)

# We're building Itcl
add_definitions(-DBUILD_itcl)

# Use fPIC if we have it
include(CheckCCompilerFlag)
check_c_compiler_flag(-fPIC HAVE_PIC_COMPILER_FLAG)
if(HAVE_PIC_COMPILER_FLAG)
  add_definitions(-fPIC)
endif(HAVE_PIC_COMPILER_FLAG)

#--------------------------------------------------------------------
# Header checks.
#--------------------------------------------------------------------

include(CMakeParseArguments)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)
function(Tcl_Check_Include_File filename)
  cmake_parse_arguments(OPT "USE" "" "" ${ARGN})
  string(REPLACE "." "_" var "${filename}")
  string(REPLACE "/" "_" var "${var}")
  string(TOUPPER "${var}" var)
  check_include_files(${filename} HAVE_${var})
  if(HAVE_${var})
    set(CVAR HAVE_${var})
    if(OPT_USE)
      # Usability test requested - we're not done yet.
      check_c_source_compiles("#include <${filename}>\nint main() {return 0;}" ${var}_USABLE)
      if(${var}_USABLE)
	set(HAVE_${var} 1 PARENT_SCOPE)
	set(${CVAR} 1 PARENT_SCOPE)
	add_definitions(-D${CVAR}=1)
      endif(${var}_USABLE)
    else(OPT_USE)
      set(HAVE_${var} 1 PARENT_SCOPE)
      set(${CVAR} 1 PARENT_SCOPE)
      add_definitions(-D${CVAR}=1)
    endif(OPT_USE)
  endif(HAVE_${var})
endfunction(Tcl_Check_Include_File)

Tcl_Check_Include_File(sys/types.h)
Tcl_Check_Include_File(sys/stat.h)
Tcl_Check_Include_File(stdlib.h)
Tcl_Check_Include_File(string.h)
Tcl_Check_Include_File(strings.h)
Tcl_Check_Include_File(memory.h)
Tcl_Check_Include_File(inttypes.h)
Tcl_Check_Include_File(stdint.h)
Tcl_Check_Include_File(unistd.h)
Tcl_Check_Include_File(net/errno.h)
Tcl_Check_Include_File(dirent.h)
Tcl_Check_Include_File(errno.h USE)
Tcl_Check_Include_File(float.h USE)
Tcl_Check_Include_File(values.h USE)
Tcl_Check_Include_File(limits.h USE)
Tcl_Check_Include_File(sys/wait.h USE)
Tcl_Check_Include_File(dlfcn.h USE)
Tcl_Check_Include_File(sys/param.h USE)

set(ITCL_SRCS
  generic/itclStubInit.c
  generic/itclStubLib.c
  generic/itcl_bicmds.c
  generic/itcl_class.c
  generic/itcl_cmds.c
  generic/itcl_ensemble.c
  generic/itcl_linkage.c
  generic/itcl_methods.c
  generic/itcl_migrate.c
  generic/itcl_objects.c
  generic/itcl_parse.c
  generic/itcl_util.c
  )

if(WIN32)
  set(ITCL_SRCS ${ITCL_SRCS} win/dllEntryPoint.c)
endif(WIN32)

set(ITCL_STUB_SRCS generic/itclStubLib.c)

set(ITCL_INCLUDE_PATH ${ITCL_SOURCE_DIR}/generic ${ITCL_BINARY_DIR}/include)
mark_as_advanced(ITCL_INCLUDE_PATH)

# ITcl requires private Tcl headers, which we can't assume from a system
# install of Tcl.  The fallback is to use our local copies and hope they
# work. (Ugh.)
if (NOT EXISTS "${TCL_INCLUDE_PATH}/tcl-private/generic")
  set(TCL_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/generic)
  if(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/unix)
  else(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/win)
  endif(NOT WIN32)
else()
  set(TCL_PRIVATE_HDRS ${TCL_INCLUDE_PATH}/tcl-private/generic)
  if(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${TCL_INCLUDE_PATH}/tcl-private/unix)
  else(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${TCL_INCLUDE_PATH}/tcl-private/win)
  endif(NOT WIN32)
endif ()

include_directories(
  ${TCL_INCLUDE_PATH}
  ${ITCL_INCLUDE_PATH}
  ${TCL_PRIVATE_HDRS}
  )

add_library(itcl${ITCL_VERSION} SHARED ${ITCL_SRCS})
target_link_libraries(itcl${ITCL_VERSION} ${M_LIBRARY} ${TCL_STUB_LIBRARY} )
set_property(TARGET itcl${ITCL_VERSION} APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
install(TARGETS itcl${ITCL_VERSION}
  RUNTIME DESTINATION ${BIN_DIR}/itcl${ITCL_VERSION}
  LIBRARY DESTINATION ${SHARED_DIR}/itcl${ITCL_VERSION}
  ARCHIVE DESTINATION ${LIB_DIR}/itcl${ITCL_VERSION})

add_library(itclstub STATIC ${ITCL_STUB_SRCS})
install(TARGETS itclstub
  RUNTIME DESTINATION ${BIN_DIR}/itcl${ITCL_VERSION}
  LIBRARY DESTINATION ${LIB_DIR}/itcl${ITCL_VERSION}
  ARCHIVE DESTINATION ${LIB_DIR}/itcl${ITCL_VERSION})
set_property(TARGET itclstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)

# Create pkgIndex.tcl files
file(GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl"
  CONTENT "
if {![package vsatisfies [package provide Tcl] 8.5]} {return}
package ifneeded Itcl ${ITCL_VERSION} [list load [file join $dir ../../${SHARED_DIR}/itcl${ITCL_VERSION} $<TARGET_FILE_NAME:itcl${ITCL_VERSION}>] Itcl]
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION ${LIB_DIR}/itcl${ITCL_VERSION})

install(FILES library/itcl.tcl DESTINATION ${LIB_DIR}/itcl${ITCL_VERSION})

set(ITCL_HDRS
  generic/itcl.h
  generic/itclDecls.h
  generic/itclInt.h
  generic/itclIntDecls.h
  )
install(FILES ${ITCL_HDRS} DESTINATION ${INCLUDE_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

