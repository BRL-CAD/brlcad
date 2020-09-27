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

find_package(TCL)

# version numbers
set(ITCL_VERSION_MAJOR 3)
set(ITCL_VERSION_MINOR 4)
set(ITCL_PATCH_LEVEL 3)

set(ITCL_VERSION "${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR}")

#---------------------------------------------------------------------
# Initialize some relative variable names
if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)
if(NOT DEFINED LIB_DIR)
  if(NOT WIN32)
    set(LIB_DIR lib)
  else(NOT WIN32)
    set(LIB_DIR bin)
  endif(NOT WIN32)
endif(NOT DEFINED LIB_DIR)
if(NOT DEFINED INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT DEFINED INCLUDE_DIR)

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
# install of Tcl.  The workaround is to use our local copies and hope they
# are workable. (Ugh.)
if (TARGET tcl OR NOT EXISTS "${TCL_INCLUDE_PATH}/tcl-private/generic")
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

add_library(itcl ${ITCL_SRCS})
target_link_libraries(itcl ${M_LIBRARY} ${TCL_STUB_LIBRARY} )
set_target_properties(itcl PROPERTIES VERSION ${ITCL_VERSION} SOVERSION ${ITCL_VERSION_MAJOR})
set_property(TARGET itcl APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
install(TARGETS itcl
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
if (TARGET tcl)
  add_dependencies(itcl tcl)
endif (TARGET tcl)

add_library(itclstub STATIC ${ITCL_STUB_SRCS})
install(TARGETS itclstub
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
set_property(TARGET itclstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)

# Create pkgIndex.tcl files
include(ITCL_PKGINDEX.cmake)
ITCL_PKGINDEX(itcl itcl ${ITCL_VERSION})
if (TARGET tclsh)
  add_dependencies(tclsh itcl_pkgIndex)
endif (TARGET tclsh)
if (TARGET wish)
  add_dependencies(wish itcl_pkgIndex)
endif (TARGET wish)


# Put library files where they need to go, and set up install rules
set(LIBRARY_FILES itcl.tcl)
foreach(libfile ${LIBRARY_FILES})
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/${libfile}
      ${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION}/${libfile} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/${libfile}
	${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/itcl${ITCL_VERSION}/${libfile} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
  install(FILES library/${libfile} DESTINATION lib/itcl${ITCL_VERSION})
endforeach(libfile ${LIBRARY_FILES})

set(ITCL_HDRS
  generic/itcl.h
  generic/itclDecls.h
  generic/itclInt.h
  generic/itclIntDecls.h
  )
install(FILES ${ITCL_HDRS} DESTINATION include)

# Set up build directory copies of the public headers
foreach(hdrfile ${ITCL_HDRS})
  get_filename_component(hf ${hdrfile} NAME)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/${hf} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${INCLUDE_DIR}/${hf} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(hdrfile ${ITCL_HDRS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

