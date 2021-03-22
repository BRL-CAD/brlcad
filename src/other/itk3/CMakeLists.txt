#                     C M A K E L I S T S . T X T
# ITK
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
# ***                    Itk CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(ITK)

find_package(TCL)

# version numbers
set(ITK_VERSION_MAJOR 3)
set(ITK_VERSION_MINOR 4)
set(ITK_PATCH_LEVEL 2)
set(ITK_VERSION "${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR}")

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

# We're building Itk
add_definitions(-DBUILD_itk)

#--------------------------------------------------------------------
# We need to enable the threading macros found in tcl.h and tclInt.h.
# The use of the threading features is determined by the core the
# extension is loaded into, but we need to compile with these macros
# turned on.
#--------------------------------------------------------------------
add_definitions(-DTCL_THREADS=1)

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

Tcl_Check_Include_File(dlfcn.h USE)
Tcl_Check_Include_File(errno.h USE)
Tcl_Check_Include_File(float.h USE)
Tcl_Check_Include_File(inttypes.h)
Tcl_Check_Include_File(limits.h USE)
Tcl_Check_Include_File(memory.h)
Tcl_Check_Include_File(net/errno.h USE)
Tcl_Check_Include_File(stdint.h)
Tcl_Check_Include_File(stdlib.h)
Tcl_Check_Include_File(string.h)
Tcl_Check_Include_File(strings.h)
Tcl_Check_Include_File(sys/param.h USE)
Tcl_Check_Include_File(sys/stat.h)
Tcl_Check_Include_File(sys/types.h)
Tcl_Check_Include_File(sys/wait.h USE)
Tcl_Check_Include_File(unistd.h)
Tcl_Check_Include_File(values.h USE)

if(TK_GRAPHICS_SYSTEM STREQUAL "x11")
  find_package(X11)
endif(TK_GRAPHICS_SYSTEM STREQUAL "x11")

set(ITK_SRCS
  generic/itkStubInit.c
  generic/itkStubLib.c
  generic/itk_archetype.c
  generic/itk_cmds.c
  generic/itk_option.c
  generic/itk_util.c
  )

if(WIN32)
  set(ITK_SRCS ${ITK_SRCS} win/dllEntryPoint.c)
endif(WIN32)

set(ITK_STUB_SRCS generic/itkStubLib.c)

set(ITK_INCLUDE_DIRS ${ITK_SOURCE_DIR}/generic ${ITK_BINARY_DIR}/include)
mark_as_advanced(ITK_INCLUDE_DIRS)

# Like ITcl, Itk requires private Tcl headers, which we can't assume from a system
# install of Tcl.  The workaround is to use our local copies and hope they
# are workable. (Ugh.)
if (TARGET tk OR NOT EXISTS "${TCL_INCLUDE_PATH}/tcl-private/generic")
    set(TCL_PRIVATE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/generic)
  if(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/unix)
  else(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/../tcl/win)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${CMAKE_CURRENT_SOURCE_DIR}/../tk/xlib)
  endif(NOT WIN32)
else()
  set(TCL_PRIVATE_HDRS ${TCL_INCLUDE_PATH}/tcl-private/generic)
  if(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${TCL_INCLUDE_PATH}/tcl-private/unix)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${TK_INCLUDE_PATH}/tk-private/xlib)
  else(NOT WIN32)
    set(TCL_PRIVATE_HDRS ${TCL_PRIVATE_HDRS} ${TCL_INCLUDE_PATH}/tcl-private/win)
  endif(NOT WIN32)
endif ()

include_directories(
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${ITCL_INCLUDE_PATH}
  ${ITK_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${TCL_PRIVATE_HDRS}
  # We can't realistically assume Itcl3 from any system install - just reference the local copy
  ${CMAKE_CURRENT_SOURCE_DIR}/../itcl/generic
  )

add_library(itk ${ITK_SRCS})
target_link_libraries(itk ${M_LIBRARY} ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${ITCL_STUB_LIBRARY})
set_property(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
set_property(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_ITCL_STUBS)
set_property(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
set_target_properties(itk PROPERTIES VERSION ${ITK_VERSION} SOVERSION ${ITK_VERSION_MAJOR})
install(TARGETS itk
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
if (TARGET tk)
  add_dependencies(itk tk)
endif (TARGET tk)
if (TARGET itcl)
  add_dependencies(itk itcl)
  add_dependencies(itk itcl_pkgIndex)
endif (TARGET itcl)


add_library(itkstub STATIC ${ITK_STUB_SRCS})
target_link_libraries(itkstub ${ITCL_STUB_LIBRARY} ${TCL_STUB_LIBRARY})
set_property(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
set_property(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_ITCL_STUBS)
set_property(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
install(TARGETS itkstub
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

include(ITK_PKGINDEX.cmake)
ITK_PKGINDEX(itk itk "${ITK_VERSION}")
if (TARGET wish)
  add_dependencies(wish itk_pkgIndex)
endif (TARGET wish)

set(ITK_LIBRARY_FILES
  library/Archetype.itk
  library/Toplevel.itk
  library/Widget.itk
  library/itk.tcl
  library/tclIndex
  )
install(FILES ${ITK_LIBRARY_FILES} DESTINATION lib/itk${ITK_VERSION})

foreach(file ${ITK_LIBRARY_FILES})
  get_filename_component(filename ${file} NAME)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${file} ${CMAKE_BINARY_DIR}/lib/itk${ITK_VERSION}/${filename} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${file} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/itk${ITK_VERSION}/${filename} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(file ${ITK_LIBRARY_FILES})

set(ITK_HDRS
  generic/itk.h
  generic/itkDecls.h
  )
install(FILES ${ITK_HDRS} DESTINATION include)

# TODO - investigate whether file(GENERATE can replace the below file copying pattern

# Set up build directory copies of the public headers
foreach(hdrfile ${ITK_HDRS})
  get_filename_component(hf ${hdrfile} NAME)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/${hf} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${INCLUDE_DIR}/${hf} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(hdrfile ${ITK_HDRS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

