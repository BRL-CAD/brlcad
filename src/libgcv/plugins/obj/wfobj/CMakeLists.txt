#               C M A K E L I S T S . T X T
#
# This file is Copyright (c) 2010-2021 United States Government as
# represented by the U.S. Army Research Laboratory.
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
# ***                libwfobj CMakeLists.txt                      ***
# *******************************************************************


# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(WFOBJ)

#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.  If we
# are a subbuild, let the parent's CMAKE_MODULE_PATH supply files before
# our own, otherwise misc/CMake takes first priority.
set(WFOBJ_CMAKE_DIR "${WFOBJ_SOURCE_DIR}/CMake")
if(WFOBJ_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${WFOBJ_CMAKE_DIR}")
else(WFOBJ_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${WFOBJ_CMAKE_DIR};${CMAKE_MODULE_PATH}")
endif(WFOBJ_IS_SUBBUILD)

#---------------------------------------------------------------------
# Define relative install locations and output directories.  Don't set
# these if they have already been set by some other means (like a
# higher level CMakeLists.txt file including this one).
# For output directories - where built library and executable
# files will be placed after building but prior to install.  The
# necessary variables change between single and multi configuration
# build systems, so it is necessary to handle both cases on a
# conditional basis.

include(${WFOBJ_CMAKE_DIR}/Path_Setup.cmake)

# Includes are in current dir
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Check if we should be using unique_ptr
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <memory>\n int main() { std::unique_ptr<int> uptr; *uptr.get() = 0; return *uptr.get(); }" HAVE_UNIQUE_PTR)
if(HAVE_UNIQUE_PTR)
  add_definitions(-DHAVE_UNIQUE_PTR)
endif(HAVE_UNIQUE_PTR)

# Find tools for generating source files
find_package(LEMON REQUIRED)
find_package(RE2C REQUIRED)
find_package(PERPLEX REQUIRED)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_SO")
set(WFOBJ_SCANNER_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_SO")
set(obj_grammar_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_SO")
PERPLEX_TARGET(
  WFOBJ_SCANNER_SO obj_rules.l
  OUT_SRC_FILE obj_rules.cpp
  OUT_HDR_FILE obj_scanner.h
  )
LEMON_TARGET(
  WFOBJ_PARSER_SO obj_grammar.yy
  OUT_SRC_FILE obj_grammar.cpp
  OUT_HDR_FILE obj_grammar.hpp
  )
ADD_PERPLEX_LEMON_DEPENDENCY(WFOBJ_SCANNER_SO WFOBJ_PARSER_SO)

set(GENERATED_INCLUDES
  ${PERPLEX_WFOBJ_SCANNER_SO_INCLUDE_DIR}
  ${LEMON_WFOBJ_PARSER_SO_INCLUDE_DIR}
  )
set(WFOBJ_SRCS
  ${LEMON_WFOBJ_PARSER_SO_SRC}
  ${PERPLEX_WFOBJ_SCANNER_SO_SRC}
  obj_util.cpp
  obj_parser.cpp
  )
add_library(libwfobj SHARED ${WFOBJ_SRCS})
set_target_properties(libwfobj PROPERTIES PREFIX "")
set_target_properties(libwfobj PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libwfobj APPEND PROPERTY COMPILE_DEFINITIONS "WFOBJ_DLL_EXPORTS")
endif(HIDE_INTERNAL_SYMBOLS)
set_property(TARGET libwfobj APPEND PROPERTY INCLUDE_DIRECTORIES "${GENERATED_INCLUDES}")

install(TARGETS libwfobj 
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
if(BUILD_STATIC_LIBS)

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_STATIC")
  set(WFOBJ_SCANNER_STATIC_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_STATIC")
  set(obj_grammar_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/WFBOBJ_SCANNER_STATIC")
  PERPLEX_TARGET(
    WFOBJ_SCANNER_STATIC obj_rules.l
    OUT_SRC_FILE obj_rules.cpp
    OUT_HDR_FILE obj_scanner.h
    )
  LEMON_TARGET(
    WFOBJ_PARSER_STATIC obj_grammar.yy
    OUT_SRC_FILE obj_grammar.cpp
    OUT_HDR_FILE obj_grammar.hpp
    )
  ADD_PERPLEX_LEMON_DEPENDENCY(WFOBJ_SCANNER_STATIC WFOBJ_PARSER_STATIC)

  set(GENERATED_INCLUDES
    ${PERPLEX_WFOBJ_SCANNER_STATIC_INCLUDE_DIR}
    ${LEMON_WFOBJ_PARSER_STATIC_INCLUDE_DIR}
    )

  set(WFOBJ_SRCS
    ${LEMON_WFOBJ_PARSER_STATIC_SRC}
    ${PERPLEX_WFOBJ_SCANNER_STATIC_SRC}
    obj_util.cpp
    obj_parser.cpp
    )

  add_library(libwfobj-static STATIC ${WFOBJ_SRCS})
  set_target_properties(libwfobj-static PROPERTIES PREFIX "")
  if(NOT MSVC)
    set_target_properties(libwfobj-static PROPERTIES OUTPUT_NAME "libwfobj")
  endif(NOT MSVC)
  set_property(TARGET libwfobj-static APPEND PROPERTY INCLUDE_DIRECTORIES "${GENERATED_INCLUDES}")
  install(TARGETS libwfobj-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
