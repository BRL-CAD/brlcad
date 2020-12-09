#    C M A K E L I S T S . T X T   F O R   S T E P C O D E
#
# This file is Copyright (c) 2010 United States Government as
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
# ***                 STEPcode's CMakeLists.txt                   ***
# *******************************************************************

# This file contains the top level CMakeLists.txt logic for the
# STEPcode software package.


project(SC)

# SC version
set(SC_VERSION_MAJOR 0)
set(SC_VERSION_MINOR 9)
set(SC_VERSION_PATCH 1)
set(SC_VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR}.${SC_VERSION_PATCH})

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0077 OLD)

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
set(SC_CMAKE_DIR "${SC_SOURCE_DIR}/cmake")
if(NOT SC_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${SC_CMAKE_DIR};${CMAKE_MODULE_PATH}")
else(NOT SC_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${SC_CMAKE_DIR}")
endif(NOT SC_IS_SUBBUILD)

# OpenBSD has its own naming conventions.  Set a platform variable based on
# the OS name so we can test for it succinctly.
if ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")
  set(OPENBSD ON)
endif ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")

# testing and compilation options, build output dirs, install dirs, uninstall, package creation, etc
include(${SC_CMAKE_DIR}/SC_Build_opts.cmake)

# SC_ADDEXEC and SC_ADDLIB macros, dllimport/export, etc
include(${SC_CMAKE_DIR}/SC_Targets.cmake)

# Macros related to paths
include(${SC_CMAKE_DIR}/SC_Paths.cmake)

# locale stuff
include(${SC_CMAKE_DIR}/SC_Locale.cmake)

# logic related to regenerating the lexer and parser source code
include(${SC_CMAKE_DIR}/SC_Regenerate.cmake)

if(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS)
  set(SC_SDAI_ADDITIONAL_EXES_SRCS "" CACHE STRING "Source files for additional executables to be linked with SDAI libs")
endif(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS)

if(NOT DEFINED SC_BUILD_SCHEMAS)
  list(APPEND CONFIG_END_MESSAGES
    "** CMake variable SC_BUILD_SCHEMAS was not set. Defaults to building ALL schemas, which will take a"
    "     while; see http://stepcode.org/mw/index.php?title=STEPcode_CMake_variables#SC_BUILD_SCHEMAS")
  #this makes SC_BUILD_SCHEMAS show up in cmake-gui
  set(SC_BUILD_SCHEMAS "ALL" CACHE string "Semicolon-separated list of paths to EXPRESS schemas to be built")
endif(NOT DEFINED SC_BUILD_SCHEMAS)

if(NOT SC_IS_SUBBUILD)
  list(APPEND CONFIG_END_MESSAGES
    ".. Don't worry about any messages above about missing headers or failed tests, as long as"
    "    you see 'Configuring done' below. Headers and features vary by compiler."
    ".. Generating step can take a while if you are building several schemas.")
endif(NOT SC_IS_SUBBUILD)

# create config headers sc_cf.h and sc_version_string.h
include(${SC_CMAKE_DIR}/SC_Config_Headers.cmake)


################

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(MSVC)
  # Disable warning for preferred usage of secure functions (example strcpy should be strcpy_s, ...)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
else()
  add_definitions(-pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long)
  if(HAVE_NULLPTR)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  endif(HAVE_NULLPTR)
endif()

include_directories(
  ${SC_SOURCE_DIR}/include
  ${SC_BINARY_DIR}/include
 )

add_subdirectory(src/base)
add_subdirectory(src/express)
add_subdirectory(src/exppp)
add_subdirectory(src/exp2cxx)
add_subdirectory(src/exp2python)
add_subdirectory(src/clstepcore)
add_subdirectory(src/cleditor)
add_subdirectory(src/cldai)
add_subdirectory(src/clutils)
add_subdirectory(src/cllazyfile)
add_subdirectory(include)
add_subdirectory(data)
if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)
add_subdirectory(doc)

# 'make core' builds everything that isn't generated. for devs.
add_custom_target(core)
if($CACHE{SC_BUILD_SHARED_LIBS})
  add_dependencies(core stepdai stepeditor exp2cxx check-express)
else()
  add_dependencies(core stepdai-static stepeditor-static exp2cxx check-express)
endif()

# CONFIG_END_MESSAGES - list of messages to be printed after everything else is done.
# THIS MUST BE LAST to ensure that they are visible to the user without scrolling.
foreach(_msg ${CONFIG_END_MESSAGES})
  message(STATUS "${_msg}")
endforeach(_msg ${CONFIG_END_MESSAGES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

