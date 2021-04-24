#                C M A K E L I S T S . T X T
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

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)
if (POLICY CMP0077)
  cmake_policy(SET CMP0077 OLD)
endif (POLICY CMP0077)

# SC version
set(SC_VERSION_MAJOR 0)
set(SC_VERSION_MINOR 9)
set(SC_VERSION_PATCH 1)
set(SC_VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR}.${SC_VERSION_PATCH})

# Set language standards
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
set(SC_CMAKE_DIR "${SC_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${SC_CMAKE_DIR}")

# OpenBSD has its own naming conventions.  Set a platform variable based on
# the OS name so we can test for it succinctly.
if ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")
  set(OPENBSD ON)
endif ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")

#---------------------------------------------------------------------
# Set up various relative path variables and build output directories
include(Path_Setup)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

# use, i.e. don't skip the full RPATH for the build tree
if(NOT DEFINED CMAKE_SKIP_BUILD_RPATH)
  set(CMAKE_SKIP_BUILD_RPATH  FALSE)
endif()

# when building, don't use the install RPATH already
# (but later on when installing)
if(NOT DEFINED CMAKE_BUILD_WITH_INSTALL_RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
endif()

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
  if(NOT DEFINED CMAKE_INSTALL_RPATH)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
  endif()
endif(NOT APPLE)
# On OSX, we need to set INSTALL_NAME_DIR instead of RPATH
# http://www.cmake.org/cmake/help/cmake-2-8-docs.html#variable:CMAKE_INSTALL_NAME_DIR
if(NOT DEFINED CMAKE_INSTALL_NAME_DIR)
  set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif()

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()


#---------------------------------------------------------------------
# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" OFF)

option(SC_PYTHON_GENERATOR "Compile exp2python" ON)
option(SC_CPP_GENERATOR "Compile exp2cxx" ON)

option(SC_MEMMGR_ENABLE_CHECKS "Enable sc_memmgr's memory leak detection" OFF)
option(SC_TRACE_FPRINTF "Enable extra comments in generated code so the code's source in exp2cxx may be located" OFF)

option(SC_ENABLE_COVERAGE "Enable code coverage test" OFF)
if (SC_ENABLE_COVERAGE AND ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "-fprofile-arcs -ftest-coverage" CACHE STRING "Extra linker flags required by code coverage" FORCE)
endif (SC_ENABLE_COVERAGE AND ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")

option(SC_ENABLE_TESTING "Enable unittesting framework" OFF)
if(SC_ENABLE_TESTING)
  if(NOT DEFINED SC_BUILD_SCHEMAS)
    set(SC_BUILD_SCHEMAS "ALL") #test all schemas, unless otherwise specified
  endif()
  include(CTest)
endif(SC_ENABLE_TESTING)

# TODO - BRL-CAD is the only known user of this option, and it will be
# transitioning to a new setup that won't need it - once that's done,
# we should just remove this option.
option(SC_SKIP_EXEC_INSTALL "Skip installing executables" OFF)
mark_as_advanced(SC_SKIP_EXEC_INSTALL)

# SC_ADDEXEC and SC_ADDLIB macros, dllimport/export, etc
include(SC_Targets)

# Macros related to paths
include(SC_Paths)

# locale stuff
include(SC_Locale)

# logic related to regenerating the lexer and parser source code
include(SC_Regenerate)

# create config headers sc_cf.h and sc_version_string.h
include(SC_Config_Headers)

if(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS)
  set(SC_SDAI_ADDITIONAL_EXES_SRCS "" CACHE STRING "Source files for additional executables to be linked with SDAI libs")
endif(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS)

if(NOT DEFINED SC_BUILD_SCHEMAS)
  list(APPEND CONFIG_END_MESSAGES
    "** CMake variable SC_BUILD_SCHEMAS was not set. Defaults to building ALL schemas, which will take a"
    "     while; see http://stepcode.org/mw/index.php?title=STEPcode_CMake_variables#SC_BUILD_SCHEMAS")
#this makes SC_BUILD_SCHEMAS show up in cmake-gui
  set(SC_BUILD_SCHEMAS "ALL" CACHE STRING "Semicolon-separated list of paths to EXPRESS schemas to be built")
endif(NOT DEFINED SC_BUILD_SCHEMAS)

if(NOT SC_IS_SUBBUILD)
  list(APPEND CONFIG_END_MESSAGES
    ".. Don't worry about any messages above about missing headers or failed tests, as long as"
    "    you see 'Configuring done' below. Headers and features vary by compiler."
    ".. Generating step can take a while if you are building several schemas.")
endif(NOT SC_IS_SUBBUILD)

# create config headers sc_cf.h
include(SC_Config_Headers)


################

if(MSVC)
  # Disable warning for preferred usage of secure functions (example strcpy should be strcpy_s, ...)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
endif()
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  add_definitions(-pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long)
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

if(NOT SC_IS_SUBBUILD)
    #-----------------------------------------------------------------------------
  # SC Packaging
  # $make package
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "STEPcode")
  set(CPACK_SET_DESTDIR "ON")
  set(CPACK_PACKAGE_VERSION_MAJOR ${SC_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${SC_VERSION_MINOR})
  set(CPACK_PACKAGE_NAME SC)
  set(CPACK_PACKAGE_CONTACT "SC Developers <scl-dev@googlegroups.com>")
  include(CPack)
endif(NOT SC_IS_SUBBUILD)


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

