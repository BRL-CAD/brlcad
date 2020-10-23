#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2020 United States Government as represented by
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
# Build file for poly2tri (above license applies to only this file -
# poly2tri is convered by its own license.)
###

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(P2T)

# Testing option
option(P2T_TESTS "Build libp2t tests" OFF)
mark_as_advanced(P2T_TESTS)
if (P2T_TESTS)
  include(CTest)
  enable_testing()
endif (P2T_TESTS)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT DEFINED BIN_DIR)
  set (BIN_DIR bin)
endif (NOT DEFINED BIN_DIR)

if (NOT DEFINED LIB_DIR)
  set (LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

if (NOT DEFINED INCLUDE_DIR)
  set (INCLUDE_DIR include)
endif (NOT DEFINED INCLUDE_DIR)

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

if (NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS OFF)
endif (NOT DEFINED BUILD_STATIC_LIBS)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(M_LIBRARY m)
endif (HAVE_M_LIBRARY)

set(SWEEP_HDRS
  poly2tri/sweep/cdt.h
  poly2tri/sweep/advancing_front.h
  poly2tri/sweep/sweep.h
  poly2tri/sweep/sweep_context.h
  )
install(FILES ${SWEEP_HDRS} DESTINATION ${INCLUDE_DIR}/poly2tri/sweep)
install(FILES poly2tri/poly2tri.h DESTINATION ${INCLUDE_DIR}/poly2tri)
install(FILES poly2tri/common/shapes.h DESTINATION ${INCLUDE_DIR}/poly2tri/common)

set(SOURCES
  poly2tri/common/shapes.cc
  poly2tri/common/utils.cc
  poly2tri/sweep/advancing_front.cc
  poly2tri/sweep/cdt.cc
  poly2tri/sweep/sweep_context.cc
  poly2tri/sweep/sweep.cc
  )

add_definitions("-DP2T_NO_GLFW")

if (BUILD_SHARED_LIBS)
  add_library(poly2tri ${SOURCES})
  target_link_libraries(poly2tri ${M_LIBRARY})
  if (MSVC)
    set_target_properties(poly2tri PROPERTIES DEFINE_SYMBOL P2T_BUILD_DLL)
    set_property(TARGET poly2tri APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_EXPORTS")
  endif (MSVC)
  install(TARGETS poly2tri
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_SHARED_LIBS)

if (BUILD_STATIC_LIBS)
  add_library(poly2tri-static STATIC ${SOURCES})
  if (MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(poly2tri-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(poly2tri-static PROPERTIES OUTPUT_NAME poly2tri)
  endif (MSVC)
  install(TARGETS poly2tri-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
