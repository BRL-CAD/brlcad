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
# ***                   tkhtml CMakeLists.txt                     ***
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
project(TKHTML)

if(NOT TCL_LIBRARY OR NOT TCL_INCLUDE_PATH OR NOT TK_LIBRARY)
  find_package(TCL)
endif(NOT TCL_LIBRARY OR NOT TCL_INCLUDE_PATH OR NOT TK_LIBRARY)
if(NOT TCL_STUB_LIBRARY)
  find_library(TCL_STUB_LIBRARY tclstub NAMES tclstub86 tclstub8.6)
endif(NOT TCL_STUB_LIBRARY)
if(NOT TK_STUB_LIBRARY)
  find_library(TK_STUB_LIBRARY tkstub NAMES tkstub86 tkstub8.6)
endif(NOT TK_STUB_LIBRARY)
if(NOT ITCL_LIBRARY)
  find_library(ITCL_LIBRARY itcl NAMES itcl4 itcl4.0.4)
endif(NOT ITCL_LIBRARY)
if(NOT ITCL_STUB_LIBRARY)
  find_library(ITCL_STUB_LIBRARY itclstub NAMES itclstub4 itclstub4.0.4)
endif(NOT ITCL_STUB_LIBRARY)

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

if(TK_SYSTEM_GRAPHICS MATCHES "x11")
  find_package(X11)
endif(TK_SYSTEM_GRAPHICS MATCHES "x11")

#----------------------------------------------------------------------------
# First, get some standard options out of the way
add_definitions(-DSTDC_HEADERS=1)

# building tkhtml
add_definitions(-DBUILD_tkhtml)

set(TKHTML_PKGNAME Tkhtml)
set(TKHTML_PKGVERSION "3.0")



add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl > ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mkdefaultstyle.tcl ${TCL_TCLSH}
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tokenlist.txt ${TCL_TCLSH}
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cssprop.c ${CMAKE_CURRENT_BINARY_DIR}/cssprop.h
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/src/cssprop.tcl
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/htmltokens.c ${CMAKE_CURRENT_SOURCE_DIR}/src/cssprop.tcl ${TCL_TCLSH}
  )

set(TKHTML_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/htmldefaultstyle.c
  ${CMAKE_CURRENT_BINARY_DIR}/cssprop.c
  src/css.c
  src/cssdynamic.c
  src/cssparser.c
  src/csssearch.c
  src/htmldecode.c
  src/htmldraw.c
  src/htmlfloat.c
  src/htmlhash.c
  src/htmlimage.c
  src/htmlinline.c
  src/htmllayout.c
  src/htmlparse.c
  src/htmlprop.c
  src/htmlstyle.c
  src/htmltable.c
  src/htmltagdb.c
  src/htmltcl.c
  src/htmltext.c
  src/htmltree.c
  src/htmlutil.c
  src/restrack.c
  src/swproc.c
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${X11_INCLUDE_DIR}
  )

add_library(Tkhtml SHARED ${TKHTML_SRCS})
if(TK_SYSTEM_GRAPHICS MATCHES "aqua")
  # Need the X emulation functions from Tk when using aqua - the stubs
  # library doesn't seem to supply them
  target_link_libraries(Tkhtml ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
else(TK_SYSTEM_GRAPHICS MATCHES "aqua")
  target_link_libraries(Tkhtml ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${X11_LIBRARIES})
endif(TK_SYSTEM_GRAPHICS MATCHES "aqua")
set_target_properties(Tkhtml PROPERTIES VERSION ${TKHTML_PKGVERSION})
set_property(TARGET Tkhtml APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
set_property(TARGET Tkhtml APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
install(TARGETS Tkhtml
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

include("${CMAKE_CURRENT_SOURCE_DIR}/tcl_pkgindex.cmake")
tcl_pkgindex(Tkhtml ${TKHTML_PKGNAME} ${TKHTML_PKGVERSION})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/doc/macros.tcl -nroff ${CMAKE_CURRENT_SOURCE_DIR}/doc/html.man > ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n
  )
add_custom_target(tkhtml_n_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tkhtml.n DESTINATION ${MAN_DIR}/mann)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

