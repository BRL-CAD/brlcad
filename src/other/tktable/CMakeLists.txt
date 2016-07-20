# Copyright (c) 2010-2016 United States Government as represented by
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
# ***                   Tktable CMakeLists.txt                     ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  cmake_policy(SET CMP0003 NEW)
  if("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    cmake_policy(SET CMP0026 OLD)
  endif("${CMAKE_VERSION}" VERSION_GREATER 2.99)
  if("${CMAKE_VERSION}" VERSION_GREATER 3.0)
    cmake_policy(SET CMP0048 NEW)
  endif("${CMAKE_VERSION}" VERSION_GREATER 3.0)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
project(TKTABLE)

if(NOT TCL_LIBRARY OR NOT TCL_INCLUDE_PATH OR NOT TK_LIBRARY)
  find_package(TCL)
endif(NOT TCL_LIBRARY OR NOT TCL_INCLUDE_PATH OR NOT TK_LIBRARY)

if(NOT TCL_STUB_LIBRARY OR NOT TK_STUB_LIBRARY)
  if(TARGET tclsh)
    message(FATAL_ERROR "We're building tclsh, but TCL_STUB_LIBRARY isn't set - this is most likely due to an error in the build system logic.")
  endif(TARGET tclsh)
  if(NOT TCL_TCLSH)
    find_program(TCL_TCLSH tclsh)
  endif(NOT TCL_TCLSH)
  if(NOT TCL_TCLSH)
    message(FATAL_ERROR "tclsh is not available and one or more stub library variables is undefined.  Either specify the location of the tcl stub libraries (e.g. -DTCL_STUB_LIBRARY=/path/to/libtclstub86.a) or identify the location of tclsh (-DTCL_TCLSH=/path/to/tclsh)")
  endif(NOT TCL_TCLSH)
  get_filename_component(TCLSH_DIR "${TCL_TCLSH}" DIRECTORY)
  get_filename_component(TCLSH_ROOT_DIR "${TCLSH_DIR}" DIRECTORY)
endif(NOT TCL_STUB_LIBRARY OR NOT TK_STUB_LIBRARY)

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
if(NOT MAN_DIR)
  set(MAN_DIR man)
endif(NOT MAN_DIR)

#----------------------------------------------------------------------------
# First, get some standard options out of the way
add_definitions(-DSTDC_HEADERS=1)
add_definitions(-DCMAKE_HEADERS=1)

# building tkhtml
add_definitions(-DBUILD_tktable)

set(TKTABLE_PKGNAME Tktable)
set(TKTABLE_PKGVERSION "2.11")

#----------------------------------------------------------------------------
find_package(X11)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/misc/tkTable_header.tcl ${CMAKE_CURRENT_SOURCE_DIR}/library/tkTable.tcl ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h.new
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/library/tkTable.tcl ${CMAKE_CURRENT_SOURCE_DIR}/misc/tkTable_header.tcl ${TCL_TCLSH}
  )
ADD_CUSTOM_TARGET(tktable_header_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h)

SET(TKTABLE_SRCS
  generic/tkTable.c
  generic/tkTableCell.c
  generic/tkTableCellSort.c
  generic/tkTableCmds.c
  generic/tkTableEdit.c
  #generic/tkTablePs.c
  generic/tkTableTag.c
  generic/tkTableUtil.c
  generic/tkTableWin.c
  )

set(TKTABLE_CFG_H_FILE ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp)
file(WRITE ${TKTABLE_CFG_H_FILE} "/**** Variables for Tktable build - replaces -D options ****/\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define PACKAGE_NAME \"${TKTABLE_PKGNAME}\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define PACKAGE_VERSION \"${TKTABLE_PKGVERSION}\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_COMMAND \"table\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_RUNTIME \"tkTable.tcl\"\n")
file(APPEND ${TKTABLE_CFG_H_FILE} "#define TBL_RUNTIME_DIR \"${CMAKE_INSTALL_PREFIX}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION}\"\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/tktable_cfg.h.tmp)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${X11_INCLUDE_DIR}
  )

add_library(Tktable SHARED ${TKTABLE_SRCS})
add_dependencies(Tktable tktable_header_gen)
target_link_libraries(Tktable ${TCL_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
set_target_properties(Tktable PROPERTIES VERSION ${TKTABLE_PKGVERSION})
install(TARGETS Tktable
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Create the pkgIndex.tcl file.
include(${CMAKE_CURRENT_SOURCE_DIR}/tcl_pkgindex.cmake)
tcl_pkgindex(Tktable ${TKTABLE_PKGNAME} ${TKTABLE_PKGVERSION})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/license.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})

set(TKTABLE_MISC
  library/tkTable.tcl
  library/tktable.py
  )

if(CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    file(COPY ${TKTABLE_MISC} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
else(CMAKE_CONFIGURATION_TYPES)
  file(COPY ${TKTABLE_MISC} DESTINATION ${CMAKE_BINARY_DIR}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
endif(CMAKE_CONFIGURATION_TYPES)

install(FILES ${TKTABLE_MISC} DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


