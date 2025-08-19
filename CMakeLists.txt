#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2025 United States Government as represented by
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
# NOTE: BRL-CAD as a collective work is distributed under the LGPL.
#       BRL-CAD's build system is under the BSD license.
#       See the COPYING file for more details.
#

# *******************************************************************
# ***                 BRL-CAD's CMakeLists.txt                    ***
# *******************************************************************
#
# This file defines the toplevel build logic for BRL-CAD.  As
# reasonably possible, proper ordering and separation of tests should
# be added to the labeled sections below as follows:
#
#   Stage 0 - project information
#   Stage 1 - define top level options
#   Stage 2 - check programs
#   Stage 3 - check compiler characteristics
#   Stage 4 - check libraries
#   Stage 5 - check headers
#   Stage 6 - check types/structures
#   Stage 7 - check functions
#   Stage 8 - check system services
#   Stage 9 - define the BRL-CAD build targets
#
# There is an output summary printed at the end to report key info
# about the final build configuration.  Details are available in the
# CMakeCache.txt file in the build directory.
#

# ***********************************************************
# *               Stage 0 of 9 - Project Info               *
# ***********************************************************

cmake_minimum_required(VERSION 3.22)

#---------------------------------------------------------------------
# BRL-CAD's version is centrally stored in include/conf/.  See HACKING
# for details on updating the version.

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/conf/MAJOR" BRLCAD_VERSION_MAJOR)
string(STRIP ${BRLCAD_VERSION_MAJOR} BRLCAD_VERSION_MAJOR)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/conf/MINOR" BRLCAD_VERSION_MINOR)
string(STRIP ${BRLCAD_VERSION_MINOR} BRLCAD_VERSION_MINOR)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/conf/PATCH" BRLCAD_VERSION_PATCH)
string(STRIP ${BRLCAD_VERSION_PATCH} BRLCAD_VERSION_PATCH)

set(BRLCAD_VERSION "${BRLCAD_VERSION_MAJOR}.${BRLCAD_VERSION_MINOR}.${BRLCAD_VERSION_PATCH}")
if(DEFINED BRLCAD_VERSION_AMEND)
  set(BRLCAD_VERSION "${BRLCAD_VERSION}-${BRLCAD_VERSION_AMEND}")
endif(DEFINED BRLCAD_VERSION_AMEND)

#######
project(
  BRLCAD
  VERSION ${BRLCAD_VERSION}
  DESCRIPTION "Open Source Solid Modeling"
  HOMEPAGE_URL "https://brlcad.org"
  LANGUAGES C CXX
)
#######

# Policy CMP0009: FILE GLOB_RECURSE calls should not follow symlinks by default
if(POLICY CMP0009)
  cmake_policy(SET CMP0009 NEW)
endif(POLICY CMP0009)

# Policy CMP0060 tells CMake to use full paths in link specifications
if(POLICY CMP0060)
  cmake_policy(SET CMP0060 NEW)
endif(POLICY CMP0060)

# Policy CMP0074 allows CMake to search
# "prefixes specified by the <PackageName>_ROOT in find_package"
# https://cmake.org/cmake/help/git-stage/policy/CMP0074.html
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif(POLICY CMP0074)

# add_test() supports arbitrary characters in test name
if(POLICY CMP0110)
  cmake_policy(SET CMP0110 NEW)
endif(POLICY CMP0110)

# cmake_dependent_option() supports full Condition Syntax
if(POLICY CMP0127)
  cmake_policy(SET CMP0127 NEW)
endif(POLICY CMP0127)

# Policy CMP0177 now normalizes all installation paths
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif(POLICY CMP0177)

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/COPYING")
  message(FATAL_ERROR "Invalid brlcad directory: ${CMAKE_SOURCE_DIR}")
endif(NOT EXISTS "${CMAKE_SOURCE_DIR}/COPYING")

if(CMAKE_ORIGINAL_SRC_DIR)
  if(NOT "${CMAKE_ORIGINAL_SRC_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message(FATAL_ERROR "Got ${CMAKE_ORIGINAL_SOURCE_DIR}/CMakeLists.txt from ${CMAKE_SOURCE_DIR}")
  endif(NOT "${CMAKE_ORIGINAL_SRC_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
endif(CMAKE_ORIGINAL_SRC_DIR)

set(CMAKE_ORIGINAL_SRC_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH "Source of original CMakeLists.txt file" FORCE)

#------------------------------------------------------------------------------
# Check if our working directory is in a symlinked path - that can cause some
# serious problems for things like install and rpath updating, so be loud.
set(PROBLEM_PATH 0)
file(REAL_PATH "${PROJECT_SOURCE_DIR}" PROJECT_REAL_SOURCE_DIR)
if (NOT "${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_REAL_SOURCE_DIR}")
  message(WARNING "PROJECT_SOURCE_DIR is \"${PROJECT_SOURCE_DIR}\" but REAL_PATH is \"${PROJECT_REAL_SOURCE_DIR}\".  Working with a symlinked alias path causes problems in subsequent file processing and is NOT recommended.")
  set(PROBLEM_PATH 1)
endif()
file(REAL_PATH "${PROJECT_BINARY_DIR}" PROJECT_REAL_BINARY_DIR)
if (NOT "${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_REAL_BINARY_DIR}")
  message(WARNING "PROJECT_BINARY_DIR is \"${PROJECT_BINARY_DIR}\" but REAL_PATH is \"${PROJECT_REAL_BINARY_DIR}\".  Working with a symlinked alias path causes problems in subsequent file processing and is NOT recommended.")
  set(PROBLEM_PATH 1)
endif()
if (PROBLEM_PATH)
  find_program(SLEEP_EXEC sleep)
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 15)
  endif(SLEEP_EXEC)
endif(PROBLEM_PATH)

#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.  If we
# are a subbuild, let the parent's CMAKE_MODULE_PATH supply files before
# our own, otherwise misc/CMake takes first priority.
set(BRLCAD_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
list(APPEND CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR}")

#---------------------------------------------------------------------
# GitHub is using .gitattributes to specify language types on files,
# so we need it if we want to correct language identifications. On the
# other hand, we DON'T want someone to use it to mark file patterns as
# text or binary - that is much too easy to get wrong, and the
# consequences can sometimes be very subtly incorrect/hard to debug.
#
# Introspect the .gitattributes file looking for problematic keywords
if(EXISTS "${BRLCAD_SOURCE_DIR}/.gitattributes")
  file(STRINGS "${BRLCAD_SOURCE_DIR}/.gitattributes" ALINES)
  set(dangerous_keywords "text;crlf;diff;eol")
  foreach(aline ${ALINES})
    if (NOT "${aline}" MATCHES "^#.*")
      foreach(kwrd ${dangerous_keywords})
	if ("${aline}" MATCHES ".*${kwrd}.*")
	  message(
	    FATAL_ERROR
	    "\nBRL-CAD does not use its .gitattributes file for identifying whether files are text or binary.  This limitation prevents subtle errors from creeping in due to inadvertent pattern matches.  See ${BRLCAD_SOURCE_DIR}/doc/git/mime_types.txt for an in-depth discussion of how to address cases where specific line endings are needed in text files.\n"
	    )
	endif()
      endforeach()
    endif()
  endforeach()
endif(EXISTS "${BRLCAD_SOURCE_DIR}/.gitattributes")

#---------------------------------------------------------------------
# Let CMake know where to look for our counting file for configuration
# passes.  It will impact whether we print certain messages
set(BRLCAD_CNT_FILE "${BRLCAD_BINARY_DIR}/CMakeTmp/BRLCAD_BUILD_COUNT")
if(NOT EXISTS ${BRLCAD_CNT_FILE})
  set(BRLCAD_PRINT_MSGS 1)
else(NOT EXISTS ${BRLCAD_CNT_FILE})
  set(BRLCAD_PRINT_MSGS 0)
endif(NOT EXISTS ${BRLCAD_CNT_FILE})

# Now that we know whether or not we're supposed to, print the CMake version
if(BRLCAD_PRINT_MSGS)
  message(STATUS "CMake version: ${CMAKE_VERSION}")
endif(BRLCAD_PRINT_MSGS)

#---------------------------------------------------------------------
# Management of build types (this normalizes or sets CMAKE_BUILD_TYPE)
include(BRLCAD_Build_Types)
InitializeBuildType()
NormalizeBuildType()

#---------------------------------------------------------------------
# Setup and checks related to system environment settings.  Some of
# these impact search results needed to set default options, so we
# do this early in the process.
include(BRLCAD_Environment_Setup)

# We do NOT want CMake looking in the User Package Registry - have encountered
# at least one case where stale or incorrect information there has resulted in
# an incorrect include directory for Eigen
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)

#---------------------------------------------------------------------
# Prepare to manage and build BRL-CAD's external dependencies. Specifies a
# specific commit hash to use in the 'bext' repository.
include(BRLCAD_ExternalDeps)
brlcad_bext_init(e810f0e7a62dd2405e9491e9a27dc679fb7d22ee)

#---------------------------------------------------------------------
# Define various utilities.
include(BRLCAD_Util)

#---------------------------------------------------------------------
# Define an option to use OBJECT libraries.  If we are building with object
# libraries, we need position independent code.
include(CMakeDependentOption)
cmake_dependent_option(
  USE_OBJECT_LIBS
  "Use OBJECT libraries"
  ON
  "NOT MSVC"
  OFF
)
mark_as_advanced(USE_OBJECT_LIBS)
if(USE_OBJECT_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif(USE_OBJECT_LIBS)

#---------------------------------------------------------------------
# Record the CMake command line arguments (more or less) in
# CMakeFiles/CMakeOutput.log
record_cmdline_args()

#---------------------------------------------------------------------
# Set up the necessary support for timing of the configuration and
# build processes
string(TIMESTAMP CONFIG_DATE "%Y%m%d")
string(TIMESTAMP CONFIG_DATESTAMP "%a, %d %b %Y %H:%M:%S UTC" UTC)

#---------------------------------------------------------------------
# Mark the time at which the configuration process began.

set(CONFIG_DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/CONFIG_DELTA_START")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DSTAMP_FILE=${CONFIG_DELTA_START} -P "${BRLCAD_CMAKE_DIR}/scripts/timestamp.cmake"
)

#---------------------------------------------------------------------
# Define relative install locations and output directories.  Don't set
# these if they have already been set by some other means (like a
# higher level CMakeLists.txt file including this one).
# For output directories - where built library and executable
# files will be placed after building but prior to install.  The
# necessary variables change between single and multi configuration
# build systems, so it is necessary to handle both cases on a
# conditional basis.

include(Path_Setup)

#---------------------------------------------------------------------
# For cleaning files as part of the distclean command, CMake needs
# to be aware of what various generators will (or might) write out
# in each build directory.
set(DISTCLEAN_OUTFILES CTestTestfile.cmake Testing/Temporary/CTestCostData.txt Testing/Temporary/LastTest.log)
if("${CMAKE_GENERATOR}" MATCHES "Make")
  set(DISTCLEAN_OUTFILES ${DISTCLEAN_OUTFILES} Makefile)
endif("${CMAKE_GENERATOR}" MATCHES "Make")
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
  set(DISTCLEAN_OUTFILES ${DISTCLEAN_OUTFILES} build.ninja rules.ninja .ninja_log)
endif("${CMAKE_GENERATOR}" MATCHES "Ninja")

#---------------------------------------------------------------------
# CMake's default "make test" target is a bit limited - define
# our own "unit" and "check" targets that automate more of the
# dependency updating process.
include(BRLCAD_Test_Wrappers)

#---------------------------------------------------------------------
# Load macros that will be used to define the BRL-CAD
# build logic
include(BRLCAD_Options)
include(BRLCAD_Targets)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

#---------------------------------------------------------------------
# print out the title with a pretty box computed to wrap around
box_print("*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***" "*")

#---------------------------------------------------------------------
# Set up include paths for generated header files.
include_directories(${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad)

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it if it exists
if(IS_DIRECTORY /usr/local)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
  if(IS_DIRECTORY /usr/local/include)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)
  endif(IS_DIRECTORY /usr/local/include)
endif(IS_DIRECTORY /usr/local)

#---------------------------------------------------------------------
# Intricacies involved with setting the install path mostly revolve
# around build type dependent install directories.  Also needs the
# current version defined.

include(BRLCAD_Install_Prefix)

#---------------------------------------------------------------------
# RPath is used on platforms such as Linux to allow programs to be
# relocatable.  It is also used to help programs run from their
# positions in the build directory.
#
# Managing this is somewhat involved - there are lots of situations
# where the logic will work because of a full path present in the
# rpath settings and mask a problem with the relative lookup.

# We want the full RPATH set in the build tree so we can run programs without
# needing to set LD_LIBRARY_PATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# We DON'T want the final install directory RPATH set in the build directory -
# it should only be set to the installation value when actually installed.
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# The following logic is what allows binaries to run successfully in the build
# directory AND install directory.
# This will need to be overridden for some targets that have a different
# relative position to LIB_DIR using the INSTALL_RPATH target property.
# However, we can cover the most common case for RPATH setting using the
# global default.
if(NOT APPLE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}:$ORIGIN/../${LIB_DIR}")
else(NOT APPLE)
  # For OSX, use the INSTALL_NAME_DIR target property
  set(CMAKE_INSTALL_RPATH "@executable_path/../${LIB_DIR}")
  set(CMAKE_INSTALL_NAME_DIR "@executable_path/../${LIB_DIR}")
endif(NOT APPLE)

#---------------------------------------------------------------------
# For certain platforms (in particular Visual C++) we want to keep some pre-defined
# flags that are commonly used in the build logic.
if(NOT DEFINED CMAKE_C_FLAGS_DEFAULT)
  set(CMAKE_C_FLAGS_DEFAULT "${CMAKE_C_FLAGS}" CACHE STRING "Default C flags" FORCE)
endif(NOT DEFINED CMAKE_C_FLAGS_DEFAULT)
mark_as_advanced(CMAKE_C_FLAGS_DEFAULT)
if(NOT DEFINED CMAKE_CXX_FLAGS_DEFAULT)
  set(CMAKE_CXX_FLAGS_DEFAULT "${CMAKE_CXX_FLAGS}" CACHE STRING "Default CXX flags" FORCE)
endif(NOT DEFINED CMAKE_CXX_FLAGS_DEFAULT)
mark_as_advanced(CMAKE_CXX_FLAGS_DEFAULT)

# OpenBSD 6.6 doesn't tolerate these flags when using #include <iostream>,
# so we have to test
#---------------------------------------------------------------------
# We need compiler support for certain C and C++ standards when we
# build.  Set CMake's flags accordingly for what source code syntax
# (i.e., -std=xxx flags) and API (i.e., -D_POSIX_C_SOURCE defines) to
# permit and utilize respectively.  Common profiles:
#
#   2011-2016: C11 & C++11 (reliably targets: 2013)
#     ISO/IEC 9899:2011 __STDC_VERSION__==201112L
#     ISO/IEC 14882:2011 __cplusplus==201103L
#     IEEE 1003.1-2008 -D_POSIX_C_SOURCE=200809L
#     Open Group Single UNIX Specification, Version 4 (2008+) -D_XOPEN_SOURCE=700
#   2014-2017: C11 & C++14 (reliably targets: 2016)
#     ISO/IEC 9899:2011 __STDC_VERSION__==201112L
#     ISO/IEC 14882:2014 __cplusplus==201402L
#     IEEE 1003.1-2008 -D_POSIX_C_SOURCE=200809L
#     Open Group Single UNIX Specification, Version 4 (2008+) -D_XOPEN_SOURCE=700
#   2017-2020: C11 & C++17 (reliably targets: 2019)
#     ISO/IEC 9899:2011 __STDC_VERSION__==201112L
#     ISO/IEC 14882:2017 __cplusplus==201703L
#     IEEE 1003.1-2008 -D_POSIX_C_SOURCE=200809L
#     Open Group Single UNIX Specification, Version 4 (2008+) -D_XOPEN_SOURCE=700
#
# Sources: https://sourceforge.net/p/predef/wiki/Standards/

include(BRLCAD_API_Flag)
set(API_FLAGS)
brlcad_api_flag("-D_POSIX_C_SOURCE=200809L" API_FLAGS)
brlcad_api_flag("-D_XOPEN_SOURCE=700" API_FLAGS)

# C
unset(C_STANDARD_FLAGS)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(C_STANDARD_FLAGS "${CMAKE_C_FLAGS_DEFAULT} ${CMAKE_C${CMAKE_C_STANDARD}_STANDARD_COMPILE_OPTION} ${API_FLAGS}")
string(STRIP "${C_STANDARD_FLAGS}" C_STANDARD_FLAGS)

# C++
unset(CXX_STANDARD_FLAGS)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(
  CXX_STANDARD_FLAGS
  "${CMAKE_CXX_FLAGS_DEFAULT} ${CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION} ${API_FLAGS}"
)
string(STRIP "${CXX_STANDARD_FLAGS}" CXX_STANDARD_FLAGS)

#---------------------------------------------------------------------
# We will need a brlcad_config.h.in file to hold all the #cmakedefine
# statements, which will in turn be used to generate a brlcad_conf.h
# file.  In autotools this process is handled by autoheader - in the
# case of CMake we wrap the CHECK_* functions and the creation of the
# entry in the brlcad_config.h.in file into one step via a macro.
#
# To avoid hitting the disk I/O any harder than necessary, we store
# the eventual contents of brlcad_config.h.in as a CMake string until
# it is ready to process.
#
# We also allow for multiple projects with this macro, in case
# subprojects are also managing a config.h.in a file of their own.

set(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")

set(CMAKE_CURRENT_PROJECT BRLCAD)

define_property(
  GLOBAL
  PROPERTY BRLCAD_CONFIG_H_CONTENTS
  BRIEF_DOCS "config.h.in contents"
  FULL_DOCS "config.h.in contents for BRL-CAD project"
)
if(NOT COMMAND CONFIG_H_APPEND)
  function(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endfunction(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)

config_h_append(BRLCAD "/**** Define statements for CMake ****/\n")
config_h_append(BRLCAD "#if !defined(BRLCADBUILD)\n")
config_h_append(BRLCAD "  #  pragma message \"Warning: included brlcad_config.h (compile-time API) without BRLCADBUILD defined\"\n"
)
config_h_append(BRLCAD "#endif\n")
config_h_append(BRLCAD "#if !defined(HAVE_CONFIG_H)\n")
config_h_append(BRLCAD "  #  pragma message \"Warning: included brlcad_config.h (compile-time API) without HAVE_CONFIG_H defined\"\n"
)
config_h_append(BRLCAD "#endif\n")
config_h_append(BRLCAD "#ifndef __CONFIG_H__\n")
config_h_append(BRLCAD "#define __CONFIG_H__\n")

# Set up some of the define statements for path information and other basics
config_h_append(BRLCAD "#define PACKAGE \"brlcad\"\n")
config_h_append(BRLCAD "#define PACKAGE_NAME \"BRL-CAD\"\n")

#----------------------------------------------------------------------
# Let our code know what the minimum active C++ standard is - behavior
# of some C++ functionality differs at runtime based on version.
config_h_append(BRLCAD "#define CXX_STANDARD ${CMAKE_CXX_STANDARD}\n")

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*        Stage 1 of 9 - Top Level Configure Options       *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

include(BRLCAD_User_Options)

# global msvc-specific configuration setup
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # Auto-reconfiguration - by default, a CMake generated build system
  # will re-run CMake if it detects that build system logic has
  # changed.  This is normally a good thing, but becomes problematic
  # when using Visual Studio - recent versions of MSVC will
  # individually prompt for a re-loading of generated solution files
  # one at a time.  Since BRL-CAD has over a thousand such files in a
  # default build, the only viable approach is to close Visual Studio,
  # re-run CMake manually, and re-open the project in Visual Studio.
  set(CMAKE_SUPPRESS_REGENERATION ON)

  # make visual studio default to a more useful output summary instead
  # of minimal lines with no compilation context.
  if(NOT DEFINED CMAKE_VERBOSE_MAKEFILE)
    set(CMAKE_VERBOSE_MAKEFILE ON)
  endif(NOT DEFINED CMAKE_VERBOSE_MAKEFILE)
endif("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*             Stage 2 of 9 - Check for Programs           *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# Load various wrapper macros for checking libraries, headers and
# functions
include(BRLCAD_CheckFunctions)

# A variety of tools, such as the benchmark utilities, need
# a Bourne shell and other commands - check for them.
include(FindShellDeps)

# If using dtrace, we will need to find it
if(BRLCAD_ENABLE_DTRACE)
  find_program(DTRACE_EXEC NAMES dtrace DOC "path to dtrace executable")
endif(BRLCAD_ENABLE_DTRACE)

# SWIG is an automatic generator of wrappers for various
# software languages
find_package(SWIG)
mark_as_advanced(SWIG_EXECUTABLE)
mark_as_advanced(SWIG_DIR)
mark_as_advanced(SWIG_VERSION)

# Doxygen is a tool for generating formatted documentation
# from structured source code comments.
include(Doxygen)
find_package(Doxygen)

# Define high level features of BRL-CAD.  We define the features here,
# to allow for the possibility that this is a Doxygen-only build (i.e.
# we won't be processing most of the build logic and are just setting
# up for our Doxygen run.
doxygen_feature("Portability Library" DESCRIPTION "Portability layer that makes sure basic low level system functionality is present across all platforms via a uniform API.")
doxygen_feature("Solid Raytracing" DESCRIPTION "Ray/geometry intersection calculations that report not just first surface hits but in/out pairs of hits to support the characterization of closed geometric volumes.")

# If we ONLY want to produce Doxygen output, there's no point in doing
# the full configure and bext build process
if(BRLCAD_DOXYGEN_ONLY)
  file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/doc")
  add_subdirectory(misc/doxygen)
  doxygen_feature_summary("${BRLCAD_BINARY_DIR}/CMakeTmp/features.dox")
  return()
endif(BRLCAD_DOXYGEN_ONLY)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*     Stage 3 of 9 - Check for Compiler Characteristics   *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)
# load our compiler testing macro definitions
include(CompilerFlags)

# Clear out most CMake-assigned defaults - We're managing
# our own compile flags, and don't (for example) want NDEBUG
# if we have debugging flags enabled for a Release build.
# At the same time, pull in any flags that have been set
# in the environment.
clear_build_flags()

# We need the C and C++ standards
set(CMAKE_C_FLAGS "${C_STANDARD_FLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_STANDARD_FLAGS}")

# try to use -pipe to speed up the compiles
check_c_flag(pipe)
check_cxx_flag(pipe)

# Enable visibility restrictions.  We have to deal with this on Windows, so
# enable it wherever we can to keep the code working across all platforms.
check_c_flag(fvisibility=hidden)
check_cxx_flag(fvisibility=hidden)
# If we can, hide internal library symbols
if(FVISIBILITY_HIDDEN_CXX_FLAG_FOUND)
  set(HIDE_INTERNAL_SYMBOLS 1)
endif(FVISIBILITY_HIDDEN_CXX_FLAG_FOUND)
if(MSVC)
  # On platforms other than MSVC, the hidden symbols are a convenience and may
  # not be supported by system lib headers.  With Visual Studio, they are a
  # necessity - define an extra flag so we know to always set them in that
  # case in order to properly link against the system libs.
  set(HIDE_INTERNAL_SYMBOLS 1)
  set(HIDE_INTERNAL_SYMBOLS_EXT 1)
endif(MSVC)

# check for -fno-strict-aliasing
# XXX - THIS FLAG IS REQUIRED if any level of optimization is
# enabled with GCC as we do use aliasing and type-punning.
check_c_flag(fno-strict-aliasing)
check_cxx_flag(fno-strict-aliasing)

# check for -fno-common (libtcl needs it on darwin)
check_c_flag(fno-common)
check_cxx_flag(fno-common)

# check for -fexceptions
# this is needed to resolve __Unwind_Resume when compiling and
# linking against openNURBS in librt for any -static binaries
check_c_flag(fexceptions)
check_cxx_flag(fexceptions)

# check for -ftemplate-depth-NN this is needed in libpc and
# other code using boost where the template instantiation depth
# needs to be increased from the default ANSI minimum of 17.
check_cxx_flag(ftemplate-depth-128)

# dynamic SSE optimizations for NURBS processing
#
# XXX disable the SSE flags for now as they can cause illegal instructions.
#     the test needs to also be tied to run-time functionality since gcc
#     may still output SSE instructions (e.g., for cross-compiling).
# CHECK_C_FLAG(msse)
# CHECK_C_FLAG(msse2)
# CHECK_C_FLAG(msse3)

# TODO - should be using this with MSVC, but it breaks the BUILD_SLEEP
# try_compile below with errors that appear to be coming from Windows
# headers??
# CHECK_C_FLAG("Za")

# 64bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64)
  check_c_flag(m64 VARS 64BIT_FLAG)
  check_c_flag("arch x86_64" VARS 64BIT_FLAG)
  check_c_flag(64 VARS 64BIT_FLAG)
  check_c_flag("mabi=64" VARS  64BIT_FLAG)
  check_c_flag(q64 VARS 64BIT_FLAG)
  if(64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
    add_new_flag(C 64BIT_FLAG ALL)
    add_new_flag(CXX 64BIT_FLAG ALL)
    add_new_flag(SHARED_LINKER 64BIT_FLAG ALL)
    add_new_flag(EXE_LINKER 64BIT_FLAG ALL)
  endif(64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64)

# 32 bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO" AND NOT MSVC)
  check_c_flag(m32 VARS 32BIT_FLAG)
  check_c_flag("arch i686" VARS 32BIT_FLAG)
  check_c_flag(32 VARS 32BIT_FLAG)
  check_c_flag("mabi=32" VARS 32BIT_FLAG)
  check_c_flag(q32 VARS 32BIT_FLAG)
  if(32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
    add_new_flag(C 32BIT_FLAG ALL)
    add_new_flag(CXX 32BIT_FLAG ALL)
    add_new_flag(SHARED_LINKER 32BIT_FLAG ALL)
    add_new_flag(EXE_LINKER 32BIT_FLAG ALL)
  endif(32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO" AND NOT MSVC)

# Debugging flags
if(BRLCAD_DEBUGGING)
  check_c_flag(g GROUPS DEBUG_C_FLAGS)
  check_cxx_flag(g GROUPS DEBUG_CXX_FLAGS)
  check_c_flag(ggdb3 GROUPS DEBUG_C_FLAGS)
  check_cxx_flag(ggdb3 GROUPS DEBUG_CXX_FLAGS)
  set(debug_config_list "ALL")
  add_new_flag(C DEBUG_C_FLAGS "${debug_config_list}")
  add_new_flag(CXX DEBUG_CXX_FLAGS "${debug_config_list}")
  # TODO - need to figure out a way to actually test linker flags
  add_new_flag(SHARED_LINKER DEBUG_C_FLAGS "${debug_config_list}")
  add_new_flag(EXE_LINKER DEBUG_C_FLAGS "${debug_config_list}")
  mark_as_advanced(DEBUG_FLAGS)
endif(BRLCAD_DEBUGGING)

# Warn if no minimum compilation linkage is set for Mac systems
if(APPLE AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
  message(
    WARNING
    "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nMACOSX_DEPLOYMENT_TARGET should be set for release builds\nto something less than the latest release for portability\ne.g., export MACOSX_DEPLOYMENT_TARGET=10.5\nSee https://github.com/brlcad/MacOSX-SDKs\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}"
  )
endif(APPLE AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")

# Most external projects have their own logic for handling
# the C inline issue - BRL-CAD needs a fine-grained approach.
# C and C++ sources require different treatment for the same build
# target, since C++11 doesn't allow inline to be re-defined.
check_c_inline(C_INLINE)
if(NOT HAVE_INLINE_KEYWORD AND HAVE___INLINE_KEYWORD)
  config_h_append(BRLCAD "#ifndef __cplusplus\n")
  config_h_append(BRLCAD "#  define inline __inline\n")
  config_h_append(BRLCAD "#endif /* !__cplusplus */\n")
elseif(HAVE_INLINE_KEYWORD)
  # this ugly hack is to avoid broken Mac OS X (10.13 era) ctype
  # headers that drop the inline keyword when compiling c99.
  config_h_append(BRLCAD "#if !defined(inline) && !defined(__cplusplus)\n")
  config_h_append(BRLCAD "#  define inline inline\n")
  config_h_append(BRLCAD "#endif /* !inline && !__cplusplus */\n")
endif(NOT HAVE_INLINE_KEYWORD AND HAVE___INLINE_KEYWORD)

# If building optimized, set _FORTIFY_SOURCE to 2.  Provides
# compile-time best-practice error checking on certain libc functions
# (e.g., memcpy), and provides run-time checks on buffer lengths and
# memory regions.  Unfortunately, glibc-1.6 made _FORTIFY_SOURCE spew
# an unquellable warning if optimization is disabled so we can't tie
# the flag to debug builds.
#
# TODO (3/2024) - is glibc-1.6 old enough now that we could fold this into the
# debug build flags?
if(${BRLCAD_OPTIMIZED} MATCHES "ON")
  config_h_append(BRLCAD "#ifndef _FORTIFY_SOURCE\n#  define _FORTIFY_SOURCE 2\n#endif\n")
endif(${BRLCAD_OPTIMIZED} MATCHES "ON")

# Enable this flag for additional reporting of undefined symbols.
# TODO: Fixing these is a work in progress.
# CHECK_C_COMPILER_FLAG("Wl,--no-undefined" NO_UNDEFINED_LINKER_FLAG)

# ******************************************************************* #
# For some tests, we need Werror to make sure the test actually fails

cmake_push_check_state(RESET)
check_c_flag(Werror VARS ERROR_FLAG)
if(ERROR_FLAG)
  set(CMAKE_REQUIRED_FLAGS "-Werror")
endif(ERROR_FLAG)

# Check whether the compiler supports __attribute__((format (__printf__, 1, 2)))
check_c_source_compiles(
  "int pf(const char *f, ...) __attribute__((format (__printf__, 1, 2))); int pf(const char *f, ...){return 1;} int main(int argc, char *argv[]) {return pf(\"%c\",'a');}"
  HAVE_PRINTF12_ATTRIBUTE
)
if(HAVE_PRINTF12_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_PRINTF12_ATTRIBUTE 1\n")
endif(HAVE_PRINTF12_ATTRIBUTE)

# Check whether the compiler supports __attribute__((format (__printf__, 2, 3)))
check_c_source_compiles(
  "int pf(void *o, const char *f, ...) __attribute__((format (__printf__, 2, 3))); int pf(void *o, const char *f, ...){return 1;} int main(int argc, char *argv[]) {return pf((void *)0, \"%c\",'a');}"
  HAVE_PRINTF23_ATTRIBUTE
)
if(HAVE_PRINTF23_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_PRINTF23_ATTRIBUTE 1\n")
endif(HAVE_PRINTF23_ATTRIBUTE)

# Check whether the compiler supports __attribute__((format (__scanf__, 2, 3)))
check_c_source_compiles(
  "int sf(void *o, const char *f, ...) __attribute__((format (__scanf__, 2, 3))); int sf(void *o, const char *f, ...){return 1;} int main(int argc, char *argv[]) {int i = 1; return sf((void *)0, \"%d\", &i);}"
  HAVE_SCANF23_ATTRIBUTE
)
if(HAVE_SCANF23_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_SCANF23_ATTRIBUTE 1\n")
endif(HAVE_SCANF23_ATTRIBUTE)

# Check whether the compiler supports __attribute__((deprecated))
check_c_source_compiles(
  "__attribute__((deprecated)) void func(void); void func(void){} void func2(void){} int main(int argc, char *argv[]) {func2(); return 0;}"
  HAVE_DEPRECATED_ATTRIBUTE
)
if(HAVE_DEPRECATED_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_DEPRECATED_ATTRIBUTE 1\n")
endif(HAVE_DEPRECATED_ATTRIBUTE)

# Check whether the compiler supports __declspec(deprecated())
check_c_source_compiles(
  "__declspec(deprecated(\"DEPRECATED\")) void func(void); void func(void){} void func2(void){} int main(int argc, char *argv[]) {func2(); return 0;}"
  HAVE_DEPRECATED_DECLSPEC
)
if(HAVE_DEPRECATED_DECLSPEC)
  config_h_append(BRLCAD "#define HAVE_DEPRECATED_DECLSPEC 1\n")
endif(HAVE_DEPRECATED_DECLSPEC)

# Check whether the compiler supports __attribute__((__noreturn__))
check_c_source_compiles(
  "#include <stdlib.h>
__attribute__((__noreturn__)) void noret(void); void noret(void){exit(1);} int main(int argc, char *argv[]) {noret(); return 0;}"
  HAVE_NORETURN_ATTRIBUTE
)
if(HAVE_NORETURN_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_NORETURN_ATTRIBUTE 1\n")
endif(HAVE_NORETURN_ATTRIBUTE)

# Check whether the compiler supports __declspec(noreturn)
check_c_source_compiles(
  "#include <stdlib.h>
__declspec(noreturn) void noret(void) {exit(1);} int main(int argc, char *argv[]) {noret(); return 0;}"
  HAVE_NORETURN_DECLSPEC
)
if(HAVE_NORETURN_DECLSPEC)
  config_h_append(BRLCAD "#define HAVE_NORETURN_DECLSPEC 1\n")
endif(HAVE_NORETURN_DECLSPEC)

# Check whether the compiler supports __attribute__((analyzer_noreturn))
check_c_source_compiles(
  "__attribute__((analyzer_noreturn)) void anoret(void); void anoret(void){return;} int main(int argc, char *argv[]) {anoret(); return 0;}"
  HAVE_ANALYZER_NORETURN_ATTRIBUTE
)
if(HAVE_ANALYZER_NORETURN_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_ANALYZER_NORETURN_ATTRIBUTE 1\n")
endif(HAVE_ANALYZER_NORETURN_ATTRIBUTE)

# Check whether the compiler supports __attribute__((always_inline))
check_c_source_compiles(
  "__attribute__((always_inline)) inline void always_inline(void); inline void always_inline(void){return;} int main(int argc, char *argv[]) {always_inline(); return 0;}"
  HAVE_ALWAYS_INLINE_ATTRIBUTE
)
if(HAVE_ALWAYS_INLINE_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_ALWAYS_INLINE_ATTRIBUTE 1\n")
endif(HAVE_ALWAYS_INLINE_ATTRIBUTE)

# Check whether the compiler supports __forceinline
check_c_source_compiles(
  "__forceinline inline void always_inline(void); inline void always_inline(void){return;} int main(int argc, char *argv[]) {always_inline(); return 0;}"
  HAVE_FORCEINLINE
)
if(HAVE_FORCEINLINE)
  config_h_append(BRLCAD "#define HAVE_FORCEINLINE 1\n")
endif(HAVE_FORCEINLINE)

# Check whether the compiler supports __attribute__((const))
check_c_source_compiles(
  "__attribute__((const)) void func(void); void func(void){return;} int main(int argc, char *argv[]) {func(); return 0;}"
  HAVE_CONST_ATTRIBUTE
)
if(HAVE_CONST_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_CONST_ATTRIBUTE 1\n")
endif(HAVE_CONST_ATTRIBUTE)

# Check whether the compiler supports __attribute__((pure))
check_c_source_compiles(
  "__attribute__((pure)) void func(void); void func(void){return;} int main(int argc, char *argv[]) {func(); return 0;}"
  HAVE_PURE_ATTRIBUTE
)
if(HAVE_PURE_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_PURE_ATTRIBUTE 1\n")
endif(HAVE_PURE_ATTRIBUTE)

# Check whether the compiler supports __attribute__((cold))
check_c_source_compiles(
  "__attribute__((cold)) void func(void); void func(void){return;} int main(int argc, char *argv[]) {func(); return 0;}"
  HAVE_COLD_ATTRIBUTE
)
if(HAVE_COLD_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_COLD_ATTRIBUTE 1\n")
endif(HAVE_COLD_ATTRIBUTE)

# Check whether the compiler supports __attribute__((hot))
check_c_source_compiles(
  "__attribute__((hot)) void func(void); void func(void){return;} int main(int argc, char *argv[]) {func(); return 0;}"
  HAVE_HOT_ATTRIBUTE
)
if(HAVE_HOT_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_HOT_ATTRIBUTE 1\n")
endif(HAVE_HOT_ATTRIBUTE)

# Check whether the compiler supports __attribute__((nonnull))
check_c_source_compiles(
  "__attribute__((nonnull)) int *func(int *); int *func(int *v){(*v)-=1; return v;} int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}"
  HAVE_NONNULL_ATTRIBUTE
)
if(HAVE_NONNULL_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_NONNULL_ATTRIBUTE 1\n")
endif(HAVE_NONNULL_ATTRIBUTE)

# Check whether the compiler supports __attribute__((warn_unused_result))
check_c_source_compiles(
  "__attribute__((warn_unused_result)) int *func(int *); int *func(int *v){(*v)-=1; return v;} int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}"
  HAVE_WARN_UNUSED_RESULT_ATTRIBUTE
)
if(HAVE_WARN_UNUSED_RESULT_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_WARN_UNUSED_RESULT_ATTRIBUTE 1\n")
endif(HAVE_WARN_UNUSED_RESULT_ATTRIBUTE)

# Check whether the compiler supports __attribute__((flatten))
check_c_source_compiles(
  "__attribute__((flatten)) int *func(int *v){(*v)-=1; return v;} __attribute((flatten)) int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}"
  HAVE_FLATTEN_ATTRIBUTE
)
if(HAVE_FLATTEN_ATTRIBUTE)
  config_h_append(BRLCAD "#define HAVE_FLATTEN_ATTRIBUTE 1\n")
endif(HAVE_FLATTEN_ATTRIBUTE)

# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care.
check_c_flag(Qunused-arguments)
check_cxx_flag(Qunused-arguments)

cmake_pop_check_state()

# The following tests are almost entirely for gcc/llvm style flags, so
# we don't unnecessarily slow down the Windows build.
if(NOT MSVC)
  # This is nominally a header test, but because it can impact the C/C++ flags
  # we do it before the final caching of BRL-CAD flags.  We need this because
  # pthread headers on some BSD platforms still haven't been scrubbed for
  # c90/posix.1 compliance - see r70785
  brlcad_include_file(pthread.h PROBE_PTHREAD_H)
  if(NOT PROBE_PTHREAD_H)
    cmake_push_check_state(RESET)
    # pthread.h on FreeBSD 10 and some older Linucies use non-c90 types
    set(CMAKE_REQUIRED_DEFINITIONS "-Dclockid_t=clock_t")
    set(CMAKE_REQUIRED_FLAGS "-pthread")
    brlcad_include_file(pthread.h PROBE_PTHREAD_H_CLOCKID_T)
    if(PROBE_PTHREAD_H_CLOCKID_T)
      set(C_STANDARD_FLAGS "${C_STANDARD_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CXX_STANDARD_FLAGS "${CXX_STANDARD_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dclockid_t=clock_t -pthread")
    endif(PROBE_PTHREAD_H_CLOCKID_T)
  endif(NOT PROBE_PTHREAD_H)

  # -O3/-Ofast can enable -ffast-math which can provoke a stack
  # corruption in the shadow computations because of strict aliasing
  # getting enabled.  we _require_ -fno-strict-aliasing until someone
  # changes how lists are managed.  -fast-math results in non-IEEE
  # floating point math among a handful of other optimizations that
  # cause substantial error in ray tracing and tessellation (and
  # probably more).

  check_c_flag(O3 GROUPS OPTIMIZE_C_FLAGS)
  check_cxx_flag(O3 GROUPS OPTIMIZE_CXX_FLAGS)

  # goes a step beyond O3, possibly using dangerous imprecise math
  # CHECK_C_FLAG(Ofast GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG(Ofast GROUPS OPTIMIZE_CXX_FLAGS)

  # binaries will not necessarily work on a different machine
  # CHECK_C_FLAG(march=native GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG(march=native GROUPS OPTIMIZE_CXX_FLAGS)

  # perform interprocedural analysis, slows compilation heavily
  check_c_flag(fipa-pta GROUPS OPTIMIZE_C_FLAGS)
  check_cxx_flag(fipa-pta GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O2, but not in older compilers
  check_c_flag(fstrength-reduce GROUPS OPTIMIZE_C_FLAGS)
  check_cxx_flag(fstrength-reduce GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O2, but not in older compilers
  check_c_flag(fexpensive-optimizations GROUPS OPTIMIZE_C_FLAGS)
  check_cxx_flag(fexpensive-optimizations GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O3, but not in older compilers
  check_c_flag(finline-functions GROUPS OPTIMIZE_C_FLAGS)
  check_cxx_flag(finline-functions GROUPS OPTIMIZE_CXX_FLAGS)

  # Profile-Guided Optimization -- this requires a two-pass compile,
  # with BRLCAD_PGO=ON and (optionally) the PGO_PATH environment
  # variable set to a temp folder

  set(pgo_flags_found "")
  if("$ENV{PGO_PATH}" STREQUAL "")
    set(BRLCAD_PGO_PATH "${CMAKE_BINARY_DIR}/profiling")
  else(NOT "$ENV{PGO_PATH}" STREQUAL "")
    set(BRLCAD_PGO_PATH "$ENV{PGO_PATH}")
  endif("$ENV{PGO_PATH}" STREQUAL "")

  if(BRLCAD_PGO AND NOT EXISTS "${BRLCAD_PGO_PATH}")
    if("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-use.*")
      message(
        FATAL_ERROR
        "PGO path ${BRLCAD_PGO_PATH} does not exist, but C flags contain the -fprofile-use flag.  This probably means you need to remove a stale CMakeCache.txt file to clear the build flags before performing the first pass of the PGO compilation.\n"
      )
    endif("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-use.*")

    # gcc-style GEN
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
      set(flags "fprofile-generate=\"${BRLCAD_PGO_PATH}\" -Wno-error=coverage-mismatch -fprofile-correction")
      set(CMAKE_REQUIRED_FLAGS "-${flags}")
      check_c_flag("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
      check_cxx_flag("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    # llvm-style GEN
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
      set(flags "fprofile-generate=\"${BRLCAD_PGO_PATH}\"")
      set(CMAKE_REQUIRED_FLAGS "-${flags}")
      check_c_flag("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
      check_cxx_flag("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    if(NOT "${pgo_flags_found}" STREQUAL "")
      distclean(${BRLCAD_PGO_PATH})
    endif(NOT "${pgo_flags_found}" STREQUAL "")
  elseif(BRLCAD_PGO AND EXISTS "${BRLCAD_PGO_PATH}")
    if(CMAKE_C_FLAGS)
      if("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-generate.*")
        message(
          FATAL_ERROR
          "PGO path ${BRLCAD_PGO_PATH} exists, but C flags contain the -fprofile-generate flag.  This probably means you need to remove a stale CMakeCache.txt file to clear the build flags before performing the second pass of the PGO compilation.\n"
        )
      endif("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-generate.*")
    endif(CMAKE_C_FLAGS)

    # gcc-style USE
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
      set(flags "fprofile-use=\"${BRLCAD_PGO_PATH}\" -Wno-error=coverage-mismatch -fprofile-correction")
      set(CMAKE_REQUIRED_FLAGS "-${flags}")
      check_c_flag("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
      check_cxx_flag("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    # llvm-style USE
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
      set(flags "fprofile-use=\"${BRLCAD_PGO_PATH}\" -Wno-profile-instr-out-of-date -Wno-profile-instr-unprofiled")
      set(CMAKE_REQUIRED_FLAGS "-${flags}")
      check_c_flag("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
      check_cxx_flag("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")
  endif(BRLCAD_PGO AND NOT EXISTS "${BRLCAD_PGO_PATH}")

  # CHECK_C_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_CXX_FLAGS)

  # link-time optimization (LTO) results in slower linking, more
  # warnings, and better optimization, but requires newer versions of
  # compilers to avoid common bugs.
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)
    # LTO bug in GCC 4.8.5 (__fread_chk_warn)
  else()
    # Try flto=auto first to avoid lto-wrapper serial compilation warnings
    # https://stackoverflow.com/questions/72218980/gcc-v12-1-warning-about-serial-compilation
    check_c_flag("flto=auto" GROUPS OPTIMIZE_C_FLAGS VARS flto_auto_c_flag_found)
    if (NOT flto_auto_c_flag_found)
      check_c_flag(flto GROUPS OPTIMIZE_C_FLAGS)
    endif()
    check_cxx_flag("flto=auto" GROUPS OPTIMIZE_CXX_FLAGS VARS flto_auto_cxx_flag_found)
    if (NOT flto_auto_cxx_flag_found)
      check_cxx_flag(flto GROUPS OPTIMIZE_CXX_FLAGS)
    endif()

    # When compiling with O3 + flto, GCC 6.3 (e.g. on Debian x64, and maybe
    # others, but I haven't tested) fails to link executables using libbrep.
    # See also: https://sourceforge.net/p/brlcad/discussion/362510/thread/676f80ce/
    #
    # The problem is that openNURBS' ~ON_SimpleArray virtual destructor (used by
    # ON_3dPointArray, for example) ends up having local binding in libbrep.so.
    # Then, when code calling it (by delete'ing an object) has to be linked, it
    # results in the error seen in the discussion thread above. libbrep.so's
    # entry for the destructor has the bogus value that is outside bounds,
    # causing the error in the thread.
    #
    # I don't know what tool causes the error (the compiler? the linker? ar?
    # ranlib?), and I don't know if it's a bug in the tool or in
    # libbrep/libOPENNUBRS, but finline-small-functions (activated by GCC at O2
    # and above) causes openNURBS' ~ON_SimpleArray virtual destructor to have
    # proper (weak) binding, allowing compilation to proceed.
    #
    # Going back through the commits, compilation started to fail after r70631,
    # which switched to C++98 from C++11.
    #
    # CHECK_CXX_FLAG(fno-inline-small-functions GROUPS OPTIMIZE_CXX_FLAGS)
  endif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)

  # GCC 4.9+ introduced "thin" LTO object files that require special
  # ar/ranlib handling via wrappers or plugin specification.  These
  # wrappers work fine even if we're not using LTO, so just use them
  # if found.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.8)
    find_program(GCC_AR_LTO ${CMAKE_C_COMPILER}-ar NAMES gcc-ar DOC "GNU ARchiver wrapper with LTO support")
    if(GCC_AR_LTO)
      set(CMAKE_AR "${GCC_AR_LTO}" CACHE FILEPATH "Archiver" FORCE)
    endif(GCC_AR_LTO)
    find_program(GCC_RANLIB_LTO ${CMAKE_C_COMPILER}-ranlib NAMES gcc-ranlib DOC "GNU Ranlib wrapper with LTO support")
    if(GCC_RANLIB_LTO)
      set(CMAKE_RANLIB "${GCC_RANLIB_LTO}" CACHE FILEPATH "Ranlib" FORCE)
    endif(GCC_RANLIB_LTO)
  endif(CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.8)
  mark_as_advanced(GCC_AR_LTO)
  mark_as_advanced(GCC_RANLIB_LTO)

  # only omit the frame pointer if we have no interest in profiling or
  # debugging.

  if(${BRLCAD_OPTIMIZED} MATCHES "ON")
    if(NOT BRLCAD_PROFILING AND NOT BRLCAD_DEBUGGING)
      check_c_flag(fomit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
      check_cxx_flag(fomit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
    else(NOT BRLCAD_PROFILING AND NOT BRLCAD_DEBUGGING)
      check_c_flag(fno-omit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
      check_cxx_flag(fno-omit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
    endif(NOT BRLCAD_PROFILING AND NOT BRLCAD_DEBUGGING)
  endif(${BRLCAD_OPTIMIZED} MATCHES "ON")

  # Add optimization flags - check if BRLCAD_OPTIMIZED is on.
  if(${BRLCAD_OPTIMIZED} MATCHES "ON")
    set(opt_conf_list "ALL")
  endif(${BRLCAD_OPTIMIZED} MATCHES "ON")
  if(opt_conf_list)
    add_new_flag(C OPTIMIZE_C_FLAGS "${opt_conf_list}")
    add_new_flag(CXX OPTIMIZE_CXX_FLAGS "${opt_conf_list}")
  endif(opt_conf_list)

  # enable stack protection for unoptimized debug builds.  this is
  # intended to help make it easier to identify problematic code but
  # only when compiling unoptimized (because the extra barrier checks
  # can affect the memory footprint and runtime performance.
  if(${BRLCAD_OPTIMIZED} MATCHES "OFF" AND BRLCAD_DEBUGGING)
    check_c_flag(fstack-protector-all)
    check_cxx_flag(fstack-protector-all)
    # checking both in case compiling c/c++ with different compilers
    check_c_flag(qstackprotect)
    check_cxx_flag(qstackprotect)
  endif(${BRLCAD_OPTIMIZED} MATCHES "OFF" AND BRLCAD_DEBUGGING)

  # Similarly, enable the AddressSanitizer memory address sanitizer.
  # See https://code.google.com/p/address-sanitizer/ for more info.
  # This typically is reported to cause a 2x slowdown.
  if(BRLCAD_ENABLE_ADDRESS_SANITIZER AND ${BRLCAD_OPTIMIZED} MATCHES "OFF" AND BRLCAD_DEBUGGING)
    set(CMAKE_REQUIRED_LINK_OPTIONS "${CMAKE_REQUIRED_LINK_OPTIONS};-lasan")
    check_c_flag(fsanitize=address)
    check_c_flag(fsanitize-address-use-after-return=always)
    check_c_flag(fsanitize-address-use-after-scope)
    check_c_flag(fno-omit-frame-pointer)
    check_cxx_flag(fsanitize=address)
    check_cxx_flag(fsanitize-address-use-after-return=always)
    check_cxx_flag(fsanitize-address-use-after-scope)
    check_cxx_flag(fno-omit-frame-pointer)
  endif(BRLCAD_ENABLE_ADDRESS_SANITIZER AND ${BRLCAD_OPTIMIZED} MATCHES "OFF" AND BRLCAD_DEBUGGING)

  # Differently, enable the ThreadSanitizer race condition detector
  # only as requested since it can incur a 5x-15x slowdown.  See
  # https://code.google.com/p/thread-sanitizer/ for more info.
  if(BRLCAD_SANITIZE_THREAD)
    check_c_flag(fsanitize=thread)
    check_cxx_flag(fsanitize=thread)
  endif(BRLCAD_SANITIZE_THREAD)

  # verbose warning flags.  we intentionally try to turn on as many as
  # possible.  adding more is encouraged (as long as all issues are
  # fixed first).
  if(BRLCAD_WARNINGS OR BRLCAD_ENABLE_STRICT)
    # also of interest:
    # -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
    # -Wformat=2 (after bu_fopen_uniq() is obsolete)
    #
    # -Werror=implicit-function-declaration -Werror=implicit-fallthrough=3 
    # -Werror=maybe-uninitialized -Werror=missing-field-initializers 
    # -Werror=incompatible-pointer-types -Werror=int-conversion 
    # -Werror=redundant-decls -Werror=parentheses -Wformat-nonliteral 
    # -Wformat-security -Winit-self -Wmaybe-uninitialized 
    # -Wold-style-definition -Wredundant-decls -Wstrict-prototypes

    check_c_flag(pedantic)
    check_cxx_flag(pedantic)

    # this catches a lot, it's good
    check_c_flag(Wall)
    check_cxx_flag(Wall)

    # this catches a lot more, also good
    check_c_flag(Wextra)
    check_cxx_flag(Wextra)

    # make sure our preprocessor logic references defined symbol names
    check_c_flag(Wundef)
    check_cxx_flag(Wundef)

    # this makes sure we don't try to compare floating point exactly
    check_c_flag(Wfloat-equal)
    check_cxx_flag(Wfloat-equal)

    # make sure we're using unambiguous symbol names, no shadowing
    check_c_flag(Wshadow)
    check_cxx_flag(Wshadow)

    # make sure we're not dangerously casting return types. C-only for
    # gcc, but maybe not for clang or others.
    check_c_flag(Wbad-function-cast)
    check_cxx_flag(Wbad-function-cast)

    # C-only: this makes sure C sources will compile as C++ code
    check_c_flag(Wc++-compat)

    # FIXME: this one is a lot of work, a work-in-progress, but good to have eventually
    # this makes sure prototypes are properly declared, no k&r and no assuming () means (void)
    # CHECK_C_FLAG(Wstrict-prototypes)

    # FIXME: shouldn't be throwing away const, should be using it more.  ton of work.
    # this reports where we throw away const
    #  CHECK_C_FLAG(Wcast-qual)
    #  CHECK_CXX_FLAG(Wcast-qual)

    # check for redundant declarations
    #  CHECK_C_FLAG(Wredundant-decls)
    #  CHECK_CXX_FLAG(Wredundant-decls)

    # want inline warnings, but not for some c++ buggy compilers
    check_c_flag(Winline)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      # g++ 4.7 spews unquellable bogus warnings on default
      # constructors (e.g., in opennurbs and boost) and there
      # are reports of problems with newer GCC as well.
      check_cxx_flag(Wno-inline)
    else()
      check_cxx_flag(Winline)
    endif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # NOTE: Cannot directly test for -Wno-... flags because GCC
    # intentionally emits no diagnostic, only warning about
    # unrecognized options when other diagnostics are produced.
    # Instead, we must use an indirect test via -Wno-error=...

    check_c_flag(Wno-error=long-long VARS long_long)
    if(long_long)
      # this makes sure we don't warn about using long long.  really, it's okay.
      check_c_flag(Wno-long-long)
      check_cxx_flag(Wno-long-long)
    endif(long_long)

    check_c_flag(Wno-error=variadic-macros VARS variadic_macros)
    if(variadic_macros)
      # this is for X11 headers, they use variadic macros
      check_c_flag(Wno-variadic-macros)
      check_cxx_flag(Wno-variadic-macros)
    endif(variadic_macros)

    # this is used to check Doxygen comment syntax
    check_c_flag(Wdocumentation)
    check_cxx_flag(Wdocumentation)
  endif(BRLCAD_WARNINGS OR BRLCAD_ENABLE_STRICT)

  if(BRLCAD_ENABLE_COVERAGE)
    # TODO: These seem to GCC specific flags - should probably set up for clang
    # as well if that's our compiler...
    # https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
    # If we need different settings for different tools, should
    # encapsulate each tool's setup in a function
    check_c_flag(ftest-coverage)
    check_cxx_flag(ftest-coverage)
    if(FTEST_COVERAGE_C_FLAG_FOUND)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage -fprofile-arcs -fprofile-update=atomic")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs -fprofile-update=atomic")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lgcov -ftest-coverage -fprofile-arcs -fprofile-update=atomic")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov -ftest-coverage -fprofile-arcs -fprofile-update=atomic")
    else(FTEST_COVERAGE_C_FLAG_FOUND)
      message(SEND_ERROR "Building with coverage is not supported by BRL-CAD on this platform.")
    endif(FTEST_COVERAGE_C_FLAG_FOUND)
  endif(BRLCAD_ENABLE_COVERAGE)

  if(BRLCAD_ENABLE_STRICT)
    # NOTE: Cannot directly test for -Wno-... flags because GCC
    # intentionally emits no diagnostic, only warning about
    # unrecognized options when other diagnostics are produced.
    # Instead, we must use an indirect test via -Wno-error=...

    # Add the flag that actually turns warnings into errors
    check_c_flag(Werror)
    check_cxx_flag(Werror)

    # Turn pedantic issues into errors also
    # FIXME: need to make sure these are not getting passed to bext
    #    check_c_flag(pedantic-errors)
    #    check_cxx_flag(pedantic-errors)

  endif(BRLCAD_ENABLE_STRICT)

  # End detection of flags intended for BRL-CAD use.  Make sure all variables have
  # their appropriate values written to the cache - otherwise, DiffCache will see
  # differences and update the COUNT file.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C Compiler flags used by all targets" FORCE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C++ Compiler flags used by all targets" FORCE)
  set(
    CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS}"
    CACHE STRING
    "Linker flags used by all shared library targets"
    FORCE
  )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Linker flags used by all exe targets" FORCE)
  mark_as_advanced(CMAKE_C_FLAGS)
  mark_as_advanced(CMAKE_CXX_FLAGS)
  mark_as_advanced(CMAKE_SHARED_LINKER_FLAGS)
  mark_as_advanced(CMAKE_EXE_LINKER_FLAGS)
  if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
    set(
      CMAKE_C_FLAGS_${BUILD_TYPE_UPPER}
      "${CMAKE_C_FLAGS_${BUILD_TYPE_UPPER}}"
      CACHE STRING
      "C Compiler flags used for ${CMAKE_BUILD_TYPE} builds"
      FORCE
    )
    set(
      CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}
      "${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}"
      CACHE STRING
      "C++ Compiler flags used for ${CMAKE_BUILD_TYPE} builds"
      FORCE
    )
    set(
      CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER}
      "${CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER}}"
      CACHE STRING
      "Linker flags used for ${CMAKE_BUILD_TYPE} builds"
      FORCE
    )
    set(
      CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER}
      "${CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER}}"
      CACHE STRING
      "Exe linker flags used for ${CMAKE_BUILD_TYPE} builds"
      FORCE
    )
    mark_as_advanced(CMAKE_C_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER})
  endif(CMAKE_BUILD_TYPE)

  # TODO - figure out if this should be integrated above
  check_compiler_flag(C "-Wunused-const-variable" HAVE_C_WUNUSED_CONST_VARIABLE)
else()
  # Whether a debug or release build, we want our own code
  # to be usable with a debugger
  add_link_options(/DEBUG)
endif(NOT MSVC)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*             Stage 4 of 9 - Check for Libraries          *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# While the primary purpose of this section is to identify libraries,
# some of the headers we are looking for are associated with the
# libraries checked here.  In those cases, we will handle the header
# logic here as opposed to separating the header logic from the
# find_package call.

# TODO - need to make LINKOPT vars for anything here that will
# be referenced in a pkgconfig file

# Look for threads (doesn't check for headers)
# Post 3.1 CMake has switched to recommending using an imported target
# and setting a "prefer pthreads" flag - previously we were using the
# CMAKE_THREAD_LIBS_INIT variable
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
# By default, the Threads package doesn't stash any of its values in
# cache.  This is inconvenient for debugging, so we set them ourselves.
set(CMAKE_HP_PTHREADS_INIT ${CMAKE_HP_PTHREADS_INIT} CACHE STRING "Threads")
set(CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT} CACHE STRING "Threads")
set(CMAKE_USE_PTHREADS_INIT ${CMAKE_USE_PTHREADS_INIT} CACHE STRING "Threads")
set(CMAKE_USE_WIN32_THREADS_INIT ${CMAKE_USE_WIN32_THREADS_INIT} CACHE STRING "Threads")
mark_as_advanced(CMAKE_HP_PTHREADS_INIT)
mark_as_advanced(CMAKE_THREAD_LIBS_INIT)
mark_as_advanced(CMAKE_USE_PTHREADS_INIT)
mark_as_advanced(CMAKE_USE_WIN32_THREADS_INIT)

# Check for the C++ STL library - need to link it explicitly in
# some compilation situations
find_package(STL)

# Check for an MPI library
set(CMAKE_Fortran_COMPILER_LOADED 0 CACHED) # silence find_package(MPI) warning
find_package(MPI COMPONENTS C)

# Check for the daemon function in -lbsd and/or -lc for adrt
brlcad_check_library(BSD bsd daemon)
brlcad_check_library(BSD c daemon)

# Mark X11 as available if it is enabled and we find Xlib.h
if(BRLCAD_ENABLE_X11)
  message("X11 detected and enabled")
  if(X11_Xlib_INCLUDE_PATH)
    set(HAVE_X11_XLIB_H 1)
    config_h_append(BRLCAD "#cmakedefine HAVE_X11_XLIB_H 1\n")
    # message("Defining HAVE_X11_XLIB_H")
  endif(X11_Xlib_INCLUDE_PATH)
  if(X11_Xi_INCLUDE_PATH)
    set(HAVE_X11_EXTENSIONS_XINPUT_H 1)
    config_h_append(BRLCAD "#cmakedefine HAVE_X11_EXTENSIONS_XINPUT_H 1\n")
    # message("Defining HAVE_X11_EXTENSIONS_XINPUT_H")
  endif(X11_Xi_INCLUDE_PATH)
endif(BRLCAD_ENABLE_X11)

# math library
brlcad_check_library(M m cos)

# uuid library
find_package(UUID)
if(UUID_FOUND)
  brlcad_check_library(UUID uuid uuid_generate)
endif(UUID_FOUND)

# network socket library (linux, bsd)
brlcad_check_library(SOCKET socket socket)

# network socket library (solaris)
brlcad_check_library(NSL nsl gethostbyaddr)

# network socket library (haiku, beos)
brlcad_check_library(NETWORK network socket)

# malloc library
brlcad_check_library(MALLOC c mallopt)
brlcad_check_library(MALLOC malloc mallopt)

# dynamic link library
brlcad_check_library(DL dl dlopen)

# Solaris lexer library
brlcad_check_library(SOLARIS_LEXER l yyless)

# Grumble... find_library doesn't find ws2_32.  Until we come up with
# working tests for these, set them by hand
if(MSVC)
  set(COMCTL32_LIBRARY comctl32.lib)
  set(IMM32_LIBRARY imm32.lib)
  # TODO - mged and adrt call this specific OpenGL library out - why doesn't
  # OPENGL_LIBRARIES do the trick?
  set(OPENGL32_LIBRARY opengl32.lib)
  set(PSAPI_LIB psapi.lib)
  set(RPCRT_LIB rpcrt4.lib)
  set(WINSOCK_LIB ws2_32.lib)
  set(WS2_32_LIBRARY ws2_32)
else(MSVC)
  set(COMCTL32_LIBRARY "")
  set(IMM32_LIBRARY "")
  set(OPENGL32_LIBRARY "")
  set(PSAPI_LIB "")
  set(RPCRT_LIB "")
  set(WINSOCK_LIB "")
  set(WS2_32_LIBRARY "")
endif(MSVC)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*               Stage 5 of 9 - Check for Headers          *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# C89 headers: assert.h, ctype.h, errno.h, float.h, limits.h, locale.h,
# math.h, setjmp.h, signal.h, stdarg.h, stddef.h, stdio.h, stdlib.h,
# string.h, time.h

# C95 headers: wchar.h, wctype.h, iso646.h

# C99 headers: complex.h, fenv.h, inttypes.h, stdbool.h, stdint.h,
# tgmath.h

# POSIX.1 headers includes C89, C95, and C99 plus the following:
# aio.h, arpa/inet.h, cpio.h, dirent.h, dlfcn.h, fcntl.h, fmtmsg.h,
# fnmatch.h, ftw.h, glob.h, grp.h, iconv.h, langinfo.h, libgen.h,
# monetary.h, mqueue.h, ndbm.h, net/if.h, netdb.h, netinet/in.h,
# netinet/tcp.h, nl_types.h, poll.h, pthread.h, pwd.h, regex.h,
# sched.h, search.h, semaphore.h, spawn.h, strings.h, stropts.h,
# sys/ipc.h, sys/mman.h, sys/msg.h, sys/resource.h, sys/select.h,
# sys/stat.h, sys/statvfs.h, sys/time.h, sys/timeb.h, sys/times.h,
# sys/types.h, sys/uio.h, sys/un.h, sys/utsname.h, sys/wait.h,
# syslog.h, tar.h, termios.h, trace.h, ucontext.h, ulimit.h, unistd.h,
# utime.h, utmpx.h, wordexp.h

# Because libtclcad, bwish and mged include Tcl headers, we need to define
# STDC_HEADERS here - the Tcl headers do need the definition.  Just
# define it - we require C89, so the test itself is not needed.
config_h_append(BRLCAD "#define STDC_HEADERS 1\n")

# AC_HEADER_SYS_WAIT
brlcad_header_sys_wait()

# dirent.h is POSIX.1, but not present on Windows (grr)
# so we need to check for it
brlcad_include_file(dirent.h HAVE_DIRENT_H)

brlcad_include_file(conio.h HAVE_CONIO_H)
brlcad_include_file(direct.h HAVE_DIRECT_H)
brlcad_include_file(dlfcn.h HAVE_DLFCN_H)
brlcad_include_file(dslib.h HAVE_DSLIB_H)
brlcad_include_file(emmintrin.h HAVE_EMMINTRIN_H)
brlcad_include_file(getopt.h HAVE_GETOPT_H)
brlcad_include_file(GL/gl.h HAVE_GL_GL_H)
brlcad_include_file(gl/device.h HAVE_GL_DEVICE_H)
brlcad_include_file(gl/glext.h HAVE_GL_GLEXT_H)
brlcad_include_file(gl/wglext.h HAVE_GL_WGLEXT_H)
brlcad_include_file(glob.h HAVE_GLOB_H)
brlcad_include_file(grp.h HAVE_GRP_H)
brlcad_include_file(inttypes.h HAVE_INTTYPES_H)
brlcad_include_file(io.h HAVE_IO_H)
brlcad_include_file(libgen.h HAVE_LIBGEN_H)
# BRLCAD_INCLUDE_FILE(libproc.h HAVE_LIBPROC_H) - see below
brlcad_include_file(mach/host_info.h HAVE_MACH_HOST_INFO_H)
brlcad_include_file(mach/mach_host.h HAVE_MACH_MACH_HOST_H)
brlcad_include_file(mach/thread_policy.h HAVE_MACH_THREAD_POLICY_H)
brlcad_include_file(memory.h HAVE_MEMORY_H)
brlcad_include_file(netdb.h HAVE_NETDB_H)
brlcad_include_file(netinet/in.h HAVE_NETINET_IN_H)
brlcad_include_file(poll.h HAVE_POLL_H)
brlcad_include_file(process.h HAVE_PROCESS_H)
brlcad_include_file(pthread.h HAVE_PTHREAD_H)
brlcad_include_file(pthread_np.h HAVE_PTHREAD_NP_H)
brlcad_include_file(pwd.h HAVE_PWD_H)
brlcad_include_file(sched.h HAVE_SCHED_H)
brlcad_include_file(sgtty.h HAVE_SGTTY_H)
brlcad_include_file(signal.h HAVE_SIGNAL_H)
brlcad_include_file(stdint.h HAVE_STDINT_H)
brlcad_include_file(stdlib.h HAVE_STDLIB_H)
brlcad_include_file(string.h HAVE_STRING_H)
brlcad_include_file(strings.h HAVE_STRINGS_H)
brlcad_include_file(strsafe.h HAVE_STRSAFE_H)

# FIXME: fails freebsd due to non-encapsulated header (__BEGIN_DECLS undecl)
brlcad_include_file(sys/cpuset.h HAVE_SYS_CPUSET_H)

brlcad_include_file(sys/file.h HAVE_SYS_FILE_H)
brlcad_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
brlcad_include_file(sys/ioctl_compat.h HAVE_SYS_IOCTL_COMPAT_H)
brlcad_include_file(sys/ipc.h HAVE_SYS_IPC_H)
brlcad_include_file(sys/machd.h HAVE_SYS_MACHD_H)
brlcad_include_file(sys/mman.h HAVE_SYS_MMAN_H)
brlcad_include_file(sys/mount.h HAVE_SYS_MOUNT_H)
brlcad_include_file(sys/param.h HAVE_SYS_PARAM_H)
brlcad_include_file(sys/prctl.h HAVE_SYS_PRCTL_H)
brlcad_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
brlcad_include_file(sys/sched.h HAVE_SYS_SCHED_H)
brlcad_include_file(sys/select.h HAVE_SYS_SELECT_H)
brlcad_include_file(sys/shm.h HAVE_SYS_SHM_H)
brlcad_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
brlcad_include_file(sys/stat.h HAVE_SYS_STAT_H)
brlcad_include_file(sys/syscall.h HAVE_SYS_SYSCALL_H)
brlcad_include_file(sys/sysctl.h HAVE_SYS_SYSCTL_H)
brlcad_include_file(sys/sysinfo.h HAVE_SYS_SYSINFO_H)
brlcad_include_file(sys/sysmp.h HAVE_SYS_SYSMP_H)
brlcad_include_file(sys/time.h HAVE_SYS_TIME_H)
brlcad_include_file(sys/times.h HAVE_SYS_TIMES_H)
brlcad_include_file(sys/types.h HAVE_SYS_TYPES_H)
brlcad_include_file(sys/uio.h HAVE_SYS_UIO_H)
brlcad_include_file(sys/un.h HAVE_SYS_UN_H)
brlcad_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)
brlcad_include_file(sys/wait.h HAVE_SYS_WAIT_H)
brlcad_include_file(syslog.h HAVE_SYSLOG_H)
brlcad_include_file(ulocks.h HAVE_ULOCKS_H)
brlcad_include_file(uuid.h HAVE_UUID_H) # for uuid_generate() on BSD
brlcad_include_file(unistd.h HAVE_UNISTD_H)
brlcad_include_file(uuid/uuid.h HAVE_UUID_UUID_H) # for uuid_generate() on Mac & Linux

# TODO - this test is failing on the github runner???
#BRLCAD_INCLUDE_FILE(windows.h HAVE_WINDOWS_H) # for QueryPerformanceCounter() on Windows
if(MSVC)
  config_h_append(BRLCAD "#define HAVE_WINDOWS_H 1\n")
  set(HAVE_WINDOWS_H 1)
endif(MSVC)

# custom sys/sysctl.h test due to BSDisms when compiling
check_c_source_compiles(
  "typedef void *rusage_info_t;\ntypedef unsigned char u_char;\ntypedef unsigned int u_int;\ntypedef unsigned long u_long;\ntypedef unsigned short u_short;\n#define SOCK_MAXADDRLEN 255\n#include <sys/types.h>\n#include <sys/sysctl.h>\nint main(void) { return 0; }"
  HAVE_SYS_SYSCTL_H
)
if(HAVE_SYS_SYSCTL_H)
  config_h_append(BRLCAD "#define HAVE_SYS_SYSCTL_H 1\n")
endif(HAVE_SYS_SYSCTL_H)

# custom libproc.h test due to BSDisms when compiling
check_c_source_compiles(
  "typedef void *rusage_info_t;\ntypedef unsigned char u_char;\ntypedef unsigned int u_int;\ntypedef unsigned long u_long;\ntypedef unsigned short u_short;\n#define SOCK_MAXADDRLEN 255\n#include <sys/types.h>\n#include <libproc.h>\nint main(void) { return 0; }"
  HAVE_LIBPROC_H
)
if(HAVE_LIBPROC_H)
  config_h_append(BRLCAD "#define HAVE_LIBPROC_H 1\n")
endif(HAVE_LIBPROC_H)

brlcad_include_file(termios.h HAVE_TERMIOS_H)
brlcad_include_file(termio.h HAVE_TERMIO_H)

# C++
brlcad_include_file_cxx(istream HAVE_ISTREAM)
brlcad_include_file_cxx(limits HAVE_LIMITS)

# Other special-case tests that need custom macros
brlcad_check_basename()
brlcad_check_dirname()

# OpenGL headers
if(BRLCAD_ENABLE_OPENGL)
  find_package(OpenGL)
  if(OPENGL_INCLUDE_DIR_GL)
    config_h_append(BRLCAD "#define HAVE_GL_GL_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GL)
  if(OPENGL_INCLUDE_DIR_GLX)
    config_h_append(BRLCAD "#define HAVE_GL_GLX_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GLX)
  if(OPENGL_USING_FRAMEWORK)
    config_h_append(BRLCAD "#define HAVE_OPENGL_GL_H 1\n")
  endif(OPENGL_USING_FRAMEWORK)
endif(BRLCAD_ENABLE_OPENGL)

# may have the header, but ensure it works in pedantic mode (gcc bug)
if(HAVE_EMMINTRIN_H)
  check_c_source_compiles("#include <emmintrin.h>\nint main(void) { return 0; }" HAVE_EMMINTRIN)
  if(HAVE_EMMINTRIN)
    config_h_append(BRLCAD "#define HAVE_EMMINTRIN 1\n")
  endif(HAVE_EMMINTRIN)
endif(HAVE_EMMINTRIN_H)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*          Stage 6 of 9 - Check for Types/Structures      *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

brlcad_struct_member("struct stat" st_blksize sys/stat.h STRUCT_STAT_ST_BLKSIZE)
brlcad_struct_member("struct stat" st_blocks sys/stat.h STRUCT_STAT_ST_BLOCKS)
brlcad_struct_member("struct stat" st_rdev sys/stat.h STRUCT_STAT_ST_RDEV)

# timespec can come in through sys/select.h
if(HAVE_SYS_SELECT_H)
  brlcad_struct_member("struct timespec" tv_sec sys/select.h STRUCT_TIMESPEC)
endif(HAVE_SYS_SELECT_H)

# socklen_t
brlcad_type_size("socklen_t" "")
if(NOT HAVE_SOCKLEN_T)
  brlcad_type_size("socklen_t" "sys/types.h")
  if(NOT HAVE_SOCKLEN_T)
    brlcad_type_size("socklen_t" "sys/socket.h")
  endif(NOT HAVE_SOCKLEN_T)
endif(NOT HAVE_SOCKLEN_T)

# FIXME: false negative on barebones linux
brlcad_type_size("cpu_set_t" "sched.h")

brlcad_type_size("int" "")
brlcad_type_size("long" "")
brlcad_type_size("long long" "")
brlcad_type_size("off_t" "")
brlcad_type_size("ptrdiff_t" "")
brlcad_type_size("size_t" "")
brlcad_type_size("ssize_t" "")
brlcad_type_size("uint64_t" "")
brlcad_type_size("uintptr_t" "")
brlcad_type_size("sig_t" "signal.h")

# see if compile settings disable historic unsigned bsd types.
# typically disabled on posix conformant systems.
brlcad_type_size("u_char" "sys/types.h")
brlcad_type_size("u_int" "sys/types.h")
brlcad_type_size("u_long" "sys/types.h")
brlcad_type_size("u_short" "sys/types.h")
if(HAVE_U_CHAR AND HAVE_U_INT AND HAVE_U_LONG AND HAVE_U_SHORT)
  config_h_append(BRLCAD "#define HAVE_U_TYPES 1\n")
endif(HAVE_U_CHAR AND HAVE_U_INT AND HAVE_U_LONG AND HAVE_U_SHORT)

# If we have an off_t that's too small, we'll need to deal with that in order to
# support large files successfully.
if(CMAKE_SIZEOF_VOID_P AND HAVE_OFF_T)
  if(${CMAKE_SIZEOF_VOID_P} GREATER ${HAVE_OFF_T})
    if(NOT DEFINED OFF_T_SIZE_MISMATCH)
      message("Note: platform void pointer size (${CMAKE_SIZEOF_VOID_P}) is greater than off_t size (${HAVE_OFF_T})")
    endif(NOT DEFINED OFF_T_SIZE_MISMATCH)
    config_h_append(BRLCAD "#define OFF_T_SIZE_MISMATCH 1\n")
    set(OFF_T_SIZE_MISMATCH 1 CACHE INTERNAL "Have off_t that is too small for platform.")
  endif(${CMAKE_SIZEOF_VOID_P} GREATER ${HAVE_OFF_T})
  if(CMAKE_WORD_SIZE STREQUAL "64BIT")
    config_h_append(BRLCAD "#define HAVE_OFF_T_64BIT 1\n")
  endif(CMAKE_WORD_SIZE STREQUAL "64BIT")
endif(CMAKE_SIZEOF_VOID_P AND HAVE_OFF_T)

# see if we have a TLS intrinsic, first check C++11 compliance
check_cxx_source_compiles("static thread_local int i = 0; int main(void) { return i; }" HAVE_THREAD_LOCAL)
if(HAVE_THREAD_LOCAL)
  config_h_append(BRLCAD "#define HAVE_THREAD_LOCAL 1\n")
else(HAVE_THREAD_LOCAL)
  # try GCC except Mac OS X
  include(CheckCXXSourceRuns)
  check_cxx_source_runs("static __thread int i = 0; int main(void) { return i; }" HAVE___THREAD)
  if(HAVE___THREAD)
    config_h_append(BRLCAD "#define HAVE___THREAD 1\n")
  else(HAVE___THREAD)
    # try Windows
    check_cxx_source_compiles(
      "static __declspec(thread) int i = 0; int main(void) { return i; }"
      HAVE___DECLSPEC_THREAD
    )
    if(HAVE___DECLSPEC_THREAD)
      config_h_append(BRLCAD "#define HAVE___DECLSPEC_THREAD 1\n")
    endif(HAVE___DECLSPEC_THREAD)
  endif(HAVE___THREAD)
endif(HAVE_THREAD_LOCAL)

# see if the compiler supports %z as a size_t print width specifier
brlcad_check_percent_z()

# see if the compiler supports [static] array hints
brlcad_check_static_arrays()

# see if we have a TLS intrinsic, first check C++11 compliance
check_cxx_source_compiles("static thread_local int i = 0; int main(void) { return i; }" HAVE_THREAD_LOCAL)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*              Stage 7 of 9 - Check for Functions         *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

include(CheckSymbolExists)

brlcad_function_exists(XQueryExtension)
brlcad_function_exists(_putenv_s)
brlcad_function_exists(_splitpath)
brlcad_function_exists(_strtoi64)
brlcad_function_exists(alarm)
brlcad_function_exists(asinh REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(confstr) # darwin/mac/linux
brlcad_function_exists(dlopen)
brlcad_function_exists(dladdr)
brlcad_function_exists(drand48)
brlcad_function_exists(fchmod)
brlcad_function_exists(fdopen)
brlcad_function_exists(fmemopen)
brlcad_function_exists(fpclassify REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(fseeko) # implies ftello
brlcad_function_exists(fsync)
brlcad_function_exists(funopen)
brlcad_function_exists(getcwd)
brlcad_function_exists(getegid)
brlcad_function_exists(getenv_s)
brlcad_function_exists(geteuid)
brlcad_function_exists(gethostname)
brlcad_function_exists(getloadavg)
brlcad_function_exists(getopt_long)
brlcad_function_exists(getpid)
brlcad_function_exists(getprogname)
brlcad_function_exists(gettimeofday)
brlcad_function_exists(htonl)
brlcad_function_exists(htonll)
brlcad_function_exists(hypot REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(isascii)
brlcad_function_exists(isinf REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(isnan REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(logb REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(lrand48)
brlcad_function_exists(lseek)
brlcad_function_exists(mkstemp)
brlcad_function_exists(modff REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(nextafter REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(nextafterf REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(ntohll)
brlcad_function_exists(pipe)
brlcad_function_exists(popen) # implies pclose
brlcad_function_exists(posix_memalign) # IEEE Std 1003.1-2001
brlcad_function_exists(proc_pidpath) # Mac OS X
brlcad_function_exists(program_invocation_name)
brlcad_function_exists(random)
brlcad_function_exists(realpath)
brlcad_function_exists(rint REQUIRED_LIBS ${M_LIBRARY})
brlcad_function_exists(setenv)
brlcad_function_exists(setpgid)
brlcad_function_exists(setpriority)
brlcad_function_exists(setsid)
brlcad_function_exists(shmat)
brlcad_function_exists(shmctl)
brlcad_function_exists(shmdt)
brlcad_function_exists(shmget)
brlcad_function_exists(snprintf)
brlcad_function_exists(srand48)
brlcad_function_exists(strcasecmp)
brlcad_function_exists(strdup)
brlcad_function_exists(strlcat)
brlcad_function_exists(strlcpy)
brlcad_function_exists(strncasecmp)
brlcad_function_exists(strtoll)
brlcad_function_exists(sync)
brlcad_function_exists(syscall)
brlcad_function_exists(sysctl)
brlcad_function_exists(sysmp)
brlcad_function_exists(time)
brlcad_function_exists(toascii)
brlcad_function_exists(tzset)
brlcad_function_exists(uname)
brlcad_function_exists(vfork)
brlcad_function_exists(vsnprintf)
brlcad_function_exists(vsscanf)
brlcad_function_exists(wait)
brlcad_function_exists(writev)

# ALLOCA test - based on AC_FUNC_ALLOCA
brlcad_alloca()

# We may want access to some POSIX functions even when compiling in strict
# mode, if they are present. For those cases, we need to record the DECL
# test result.
brlcad_function_exists(kill)
brlcad_function_exists(fileno)

# test for daemon(), which is deprecated on Mac OS X 10.5+
brlcad_function_exists(daemon)
if(HAVE_DAEMON)
  check_c_source_compiles(
    "#include <stdlib.h>\nint main(void) { (void)daemon; return 0; }"
    HAVE_WORKING_DAEMON_FUNCTION
  )
  if(HAVE_WORKING_DAEMON_FUNCTION)
    config_h_append(BRLCAD "#define HAVE_WORKING_DAEMON_FUNCTION 1\n")
  endif(HAVE_WORKING_DAEMON_FUNCTION)
endif(HAVE_DAEMON)

# sysconf may not always define everything we want on various
# systems - do some extra checks
brlcad_function_exists(sysconf)
if(HAVE_SYSCONF)
  check_c_source_compiles(
    "#include <unistd.h>\nint main(void) { (void)sysconf(_SC_AVPHYS_PAGES); return 0; }"
    HAVE_SYSCONF_AVPHYS
  )
  if(HAVE_SYSCONF_AVPHYS)
    config_h_append(BRLCAD "#define HAVE_SYSCONF_AVPHYS 1\n")
  endif(HAVE_SYSCONF_AVPHYS)
endif(HAVE_SYSCONF)

# ntohll and htonll may be harder to spot - do some extra tests
if("${HAVE_NTOHLL}" STREQUAL "")
  check_symbol_exists(ntohll "sys/_endian.h" HAVE_NTOHLL_SYS__ENDIAN)
  if(HAVE_NTOHLL_SYS__ENDIAN)
    set(HAVE_NTOHLL 1 CACHE INTERNAL "Have function NTOHLL")
    config_h_append(BRLCAD "#cmakedefine HAVE_NTOHLL 1\n")
  endif(HAVE_NTOHLL_SYS__ENDIAN)
endif("${HAVE_NTOHLL}" STREQUAL "")
if("${HAVE_HTONLL}" STREQUAL "")
  check_symbol_exists(htonll "sys/_endian.h" HAVE_HTONLL_SYS__ENDIAN)
  if(HAVE_HTONLL_SYS__ENDIAN)
    set(HAVE_HTONLL 1 CACHE INTERNAL "Have function HTONLL")
    config_h_append(BRLCAD "#cmakedefine HAVE_HTONLL 1\n")
  endif(HAVE_HTONLL_SYS__ENDIAN)
endif("${HAVE_HTONLL}" STREQUAL "")

# Test for _snprintf() if we don't have snprintf
if(NOT HAVE_SNPRINTF)
  brlcad_function_exists(_snprintf)
  if(HAVE__SNPRINTF)
    config_h_append(BRLCAD "#define snprintf _snprintf\n")
  endif(HAVE__SNPRINTF)
endif(NOT HAVE_SNPRINTF)

if(HAVE_REALPATH AND NOT HAVE_WORKING_REALPATH)
  check_c_source_runs(
    "#include<limits.h>\n#include <stdlib.h>\nint main(void) { char dir[PATH_MAX]; const char *d = \"/tmp/REALPATH_TEST_PATH\"; d = (const char *)realpath(d, dir); return 0; }"
    HAVE_WORKING_REALPATH
  )
  if(HAVE_WORKING_REALPATH)
    config_h_append(BRLCAD "#define HAVE_WORKING_REALPATH 1\n")
  endif(HAVE_WORKING_REALPATH)
endif(HAVE_REALPATH AND NOT HAVE_WORKING_REALPATH)

# GetFullPathName
check_c_source_compiles(
  "
#include <windows.h>
int main(void) {
const char *path = \"Windows\";
char *resolved_path;
(void)GetFullPathName(path, MAX_PATH, resolved_path, NULL);
return 0;
}
"
  HAVE_GETFULLPATHNAME
)
if(HAVE_GETFULLPATHNAME)
  config_h_append(BRLCAD "#define HAVE_GETFULLPATHNAME 1\n")
endif(HAVE_GETFULLPATHNAME)

# If we have GetProcessTimes, we need it instead of clock() for CPU time.
# https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/clock
check_c_source_compiles(
  "
#include <windows.h>
int main(void) {
FILETIME a,b,c,d;
(void)GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d);
return 0;
}
"
  HAVE_GETPROCESSTIMES
)
if(HAVE_GETPROCESSTIMES)
  config_h_append(BRLCAD "#define HAVE_GETPROCESSTIMES 1\n")
endif(HAVE_GETPROCESSTIMES)

# GetCurrentProcessId
check_c_source_compiles(
  "
#include <windows.h>
int main(void) {
DWORD cpid = GetCurrentProcessId();
return 0;
}
"
  HAVE_GETCURRENTPROCESSID
)
if(HAVE_GETCURRENTPROCESSID)
  config_h_append(BRLCAD "#define HAVE_GETCURRENTPROCESSID 1\n")
endif(HAVE_GETCURRENTPROCESSID)

# functionality tests for lrint
set(lrint_test "long int lrint(double); int main(void) {return lrint(3.14);}")
set(lrint_test_negative "long int lrint(double); int main(void) {return lrint(-1);}")
# decl test for lrint
set(lrint_decl_test "#include <math.h>\nint main(void) { (void)lrint(0); return 0; }")
brlcad_function_exists(lrint DECL_TEST_SRCS lrint_decl_test
  WORKING_TEST_SRCS lrint_test lrint_test_negative
  REQUIRED_LIBS ${M_LIBRARY}
)
if(HAVE_DECL_LRINT)
  config_h_append(BRLCAD "#cmakedefine HAVE_DECL_LRINT 1\n")
endif(HAVE_DECL_LRINT)
if(NOT HAVE_LRINT AND NOT DEFINED HAVE_WORKING_LRINT_MACRO)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES ${M_LIBRARY})
  # NOTE: this must match the lrint #define in include/common.h
  check_c_source_compiles(
    "#define lrint(_x) (((_x) < 0.0) ? ceil((_x)-0.5) : floor((_x)+0.5))\nint main(void) {return lrint(3.14);}"
    HAVE_WORKING_LRINT_MACRO
  )
endif(NOT HAVE_LRINT AND NOT DEFINED HAVE_WORKING_LRINT_MACRO)
if(HAVE_WORKING_LRINT_MACRO)
  config_h_append(BRLCAD "#cmakedefine HAVE_WORKING_LRINT_MACRO 1\n")
endif(HAVE_WORKING_LRINT_MACRO)

# test for tgamma
set(tgamma_test "#include <math.h>\nint main(void) {double tga = tgamma(3.14); return 0;}")
brlcad_function_exists(tgamma COMPILE_TEST_SRCS tgamma_test REQUIRED_LIBS ${M_LIBRARY})

# test for gethostbyaddr
set(gethostbyaddr_test "#include<stdlib.h>\n#include <netdb.h>\nint main(void) {(void)gethostbyaddr; return 0;}")
set(GETHOSTBYADDR_REQUIRED_FLAGS "-Werror")
brlcad_function_exists(gethostbyaddr DECL_TEST_SRCS gethostbyaddr_test REQUIRED_LIBS ${NL_LIBRARY})

# test for gethostbyname
set(gethostbyname_test "#include<stdlib.h>\n#include <netdb.h>\nint main(void) {(void)gethostbyname; return 0;}")
set(GETHOSTBYNAME_REQUIRED_FLAGS "-Werror")
brlcad_function_exists(gethostbyname DECL_TEST_SRCS gethostbyname_test REQUIRED_LIBS ${NL_LIBRARY})

# Check for logb in C++
check_cxx_source_compiles("#include <cmath>\nint main(void) {return logb(3.14);}" HAVE_CXX_LOGB)
config_h_append(BRLCAD "#cmakedefine HAVE_CXX_LOGB 1\n")

#-----------------------------------------------------------------------

# On Windows, we need to check for hypot etc.  This test pertains
# to the windows specific config file, not CONFIG_H_FILE - hence,
# just run the test and it will be handled by configure_file later.
if(WIN32)
  # consider all warnings as errors (MSVC)
  # CHECK_C_FLAG(WX)
  # CHECK_CXX_FLAG(WX)

  check_symbol_exists(hypot "math.h" HAVE_HYPOT)
  if(NOT HAVE_HYPOT)
    set(hypot 1)
  endif(NOT HAVE_HYPOT)

  check_symbol_exists(asinh "math.h" HAVE_ASINH)
  if(NOT HAVE_ASINH)
    set(asinh 1)
  endif(NOT HAVE_ASINH)

  check_symbol_exists(isnan "math.h" HAVE_ISNAN)
  if(HAVE_ISNAN)
    config_h_append(BRLCAD "#define HAVE_ISNAN 1\n")
  else(HAVE_ISNAN)
    check_symbol_exists(_isnan "float.h" HAVE__ISNAN)
    if(HAVE__ISNAN)
      config_h_append(BRLCAD "#define HAVE__ISNAN 1\n")
    endif(HAVE__ISNAN)
  endif(HAVE_ISNAN)

  check_symbol_exists(isinf "math.h" HAVE_ISINF)
  if(NOT HAVE_ISINF)
    set(isinf 1)
  endif(NOT HAVE_ISINF)

  check_symbol_exists(rint "math.h" HAVE_RINT)

  check_symbol_exists(fmax "math.h" HAVE_FMAX)
  if(NOT HAVE_FMAX)
    set(fmax 1)
  endif(NOT HAVE_FMAX)

  check_symbol_exists(nextafterf "math.h" HAVE_NEXTAFTERF)
  if(NOT HAVE_NEXTAFTERF)
    set(nextafterf 1)
  endif(NOT HAVE_NEXTAFTERF)

  check_symbol_exists(nextafterl "math.h" HAVE_NEXTAFTERL)
  if(NOT HAVE_NEXTAFTERL)
    set(nextafterl 1)
  endif(NOT HAVE_NEXTAFTERL)

  brlcad_function_exists(_fseeki64)
  brlcad_function_exists(_lseeki64)
  brlcad_function_exists(_ftelli64)
endif(WIN32)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*          Stage 8 of 9 - Check for System Services       *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# COUNT - Count how many times the configuration has changed.
set(buildCounter 0)
if(EXISTS "${BRLCAD_CNT_FILE}")
  file(READ "${BRLCAD_CNT_FILE}" buildCounter_raw)
  string(STRIP ${buildCounter_raw} buildCounter)
  math(EXPR buildCounter "${buildCounter} + 1")
  file(WRITE "${BRLCAD_CNT_FILE}" "${buildCounter}\n")
else(EXISTS "${BRLCAD_CNT_FILE}")
  file(WRITE "${BRLCAD_CNT_FILE}" "${buildCounter}\n")
endif(EXISTS "${BRLCAD_CNT_FILE}")
distclean("${BRLCAD_CNT_FILE}")
set(BRLCAD_COMPILE_COUNT ${buildCounter})

# DATE - RFC2822 timestamp
set(BRLCAD_COMPILE_DATE \"${CONFIG_DATESTAMP}\")

# HOST
cmake_host_system_information(RESULT BRLCAD_COMPILE_HOSTNAME QUERY HOSTNAME)
string(STRIP "${BRLCAD_COMPILE_HOSTNAME}" BRLCAD_COMPILE_HOSTNAME)

# USER
set(BRLCAD_COMPILE_USER $ENV{USERNAME})
string(STRIP "${BRLCAD_COMPILE_USER}" BRLCAD_COMPILE_USER)

if(MSVC)
  # By default, do not warn when built on machines using only VS Express
  # From: http://www.cmake.org/pipermail/cmake/2011-May/044166.html
  if(NOT DEFINED CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
  endif()
  # For Windows, we need some dlls to be redistributed with the installer.
  include(InstallRequiredSystemLibraries)

  # Enable utf-8 support - apparently this is not enabled by default in Visual
  # Studio in all situations:
  # https://learn.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

  # Before we finalize, set some specific global linker flags
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
endif(MSVC)

#-----------------------------------------------------------------------------
# Add misc/tools for tools that are used in BRL-CAD's build process or for
# maintenance activities.
add_subdirectory(misc/tools)

# Now that the user options are set and we have misc/tools... if we're going to
# be bundling 3rd party dependencies, we need to get them staged - some of the
# build steps will require that dependencies work during the build process, and
# the most reliably way to do that is to get them in place in the build
# directory.  Because the feature flags may impact what we're looking to use,
# we wait until they are set to do the find_package calls.
#
# The way BRL-CAD handles bundled dependencies is simple - if a dependency
# is found in the specified BRLCAD_EXT_DIR directory by find_package, that
# version is used in preference to a system version.  If no bundled version
# is found the standard system search logic will kick in.  (System versions
# will NOT be bundled with BRL-CAD.)
#
# Note that while feature flags can change which elements of BRL-CAD are built,
# they WON'T change what's bundled with BRL-CAD for install - that's based
# strictly on the contents of the user's specified external dependencies
# directory.  If (for example) STEP support is disabled, this logic will not
# skip bundling the STEPCODE external dependencies - the user should set up an
# external dependencies repository without that component if they don't want to
# bundle it.
#
# This logic will also set up the necessary steps to install the bundling
# targets as part of BRL-CAD's install process.
#
# The exception to the above indifference to user settings is when BRL-CAD's
# configure logic undertakes the building of the dependencies itself, rather
# than using a pre-built bext repository.  In that situation, enough info is
# available to make intelligent decisions about what to turn on and off at
# configure time, and we don't have to solve the problem of figuring out which
# output files correspond to which project(s).  To allow us to also pass
# detailed info about which components are enabled to the bext build process,
# we need to know from the BRL-CAD build what component dependencies are.
# Since we can't add_subdirectory src/ until we have the dependencies set
# up, we have a chicken-egg problem.  To resolve it, we break the dependency
# info out into a separate file used both here and by src/ :
include(${CMAKE_SOURCE_DIR}/src/source_dirs.cmake)

# Now we have the necessary info - trigger the primary management logic
brlcad_bext_process()

# Define some Tcl related variables for libtclcad
if(BRLCAD_ITCL_BUILD)
  config_h_append(BRLCAD "#cmakedefine ITCL_VERSION \"${ITCL_VERSION}\"\n")
endif(BRLCAD_ITCL_BUILD)
if(BRLCAD_ITK_BUILD)
  config_h_append(BRLCAD "#cmakedefine ITK_VERSION \"${ITK_VERSION}\"\n")
endif(BRLCAD_ITK_BUILD)
if(DEFINED IWIDGETS_VERSION)
  set(IWIDGETS_VERSION "4.1.1")
  config_h_append(BRLCAD "#cmakedefine IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
endif(DEFINED IWIDGETS_VERSION)

if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*        Stage 9 of 9 - Output and Summarize Config       *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# If we're building Docbook documentation, we need to prepare
# some settings before the BRLCAD_ADDEXEC targets are defined
if(BRLCAD_EXTRADOCS)
  # Define a build target that builds a no-op executable, so we can ensure
  # a target for generator expression use regardless of other changes elsewhere
  # in the build system
  add_executable(db_dir_info doc/docbook/db_dir_info.c)
  set_target_properties(db_dir_info PROPERTIES FOLDER "DocBook")

  # Most of the Docbook resources we need are defined here
  add_subdirectory(doc/docbook/resources)

  # Rework CMake binary and source paths for DocBook templates - if we
  # have spaces in the paths, they will cause a problem.
  string(REPLACE " " "%20" DOCBOOK_BINARY_DIR "${CMAKE_BINARY_DIR}")
  string(REPLACE " " "%20" DOCBOOK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

  # Files for PDF products
  if(BRLCAD_EXTRADOCS_PDF)
    # Apache FOP needs a couple of config files set up
    set(srcdir ${CMAKE_CURRENT_SOURCE_DIR}/doc/docbook)
    configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
    distclean("${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf")
  endif(BRLCAD_EXTRADOCS_PDF)

  # For the html files, we need brlcad.css
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/docbook/brlcad_css-done
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_SOURCE_DIR}/doc/docbook/css/brlcad.css
      $<TARGET_FILE_DIR:db_dir_info>/${RBIN_DIR}/${DOC_DIR}/html/css/brlcad.css
    COMMAND "${CMAKE_COMMAND}" -E touch ${CMAKE_BINARY_DIR}/doc/docbook/brlcad_css-done
    DEPENDS ${CMAKE_SOURCE_DIR}/doc/docbook/css/brlcad.css
  )
  add_custom_target(brlcad_css DEPENDS ${CMAKE_BINARY_DIR}/doc/docbook/brlcad_css-done)
  set_target_properties(brlcad_css PROPERTIES FOLDER "DocBook")
  install(FILES ${CMAKE_SOURCE_DIR}/doc/docbook/css/brlcad.css DESTINATION ${DOC_DIR}/html/css)
  distclean(${DOC_DIR}/html/css/brlcad.css)

  # Include CMake macros for DocBook.
  include(DocBook)

  # The system man pages are the ones build targets might generate, so
  # make sure to set their stylesheet resources up front

  # Style sheet for XSLT transformation to HTML pages
  set(D_RDIR ${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad)
  set(D_BRDIR ${CMAKE_BINARY_DIR}/doc/docbook/resources/brlcad)
  if(BRLCAD_EXTRADOCS_HTML)
    configure_file(${D_RDIR}/brlcad-man-xhtml-stylesheet.xsl.in ${D_BRDIR}/brlcad-man-xhtml-stylesheet.xsl)
    distclean("${D_BRDIR}/brlcad-man-xhtml-stylesheet.xsl")
  endif(BRLCAD_EXTRADOCS_HTML)

  if(BRLCAD_EXTRADOCS_PHP)
    configure_file(${D_RDIR}/brlcad-man-xhtml-stylesheet.xsl.in ${D_BRDIR}/brlcad-man-xhtml-stylesheet.xsl)
    distclean("${D_BRDIR}/brlcad-man-xhtml-stylesheet.xsl")
  endif(BRLCAD_EXTRADOCS_PHP)

  # Style sheet for XSLT transformation to manual pages
  if(BRLCAD_EXTRADOCS_MAN)
    configure_file(${D_RDIR}/brlcad-man-stylesheet.xsl.in ${D_BRDIR}/brlcad-man-stylesheet.xsl)
    distclean("${D_BRDIR}/brlcad-man-stylesheet.xsl")
  endif(BRLCAD_EXTRADOCS_MAN)

  # Files for PDF
  if(BRLCAD_EXTRADOCS_PDF)
    configure_file(${D_RDIR}/brlcad-man-fo-stylesheet.xsl.in ${D_BRDIR}/brlcad-man-fo-stylesheet.xsl)
    distclean("${D_BRDIR}/brlcad-man-fo-stylesheet.xsl")
  endif(BRLCAD_EXTRADOCS_PDF)

  # For HTML, MAN and FO (FO is an intermediate file used in the
  # XML->PDF transformation) we use variables to hold the full
  # stylesheet path. In the case we need to further
  # customize FO stylesheets we can have separate CMake logic in
  # appropriate directories to handle the customization (e.g., the
  # BRL-CAD manuals in books/en/CMakeLists.txt).
  set(XSL_PHP_STYLESHEET "${D_BRDIR}/wordpress.xsl")
  set(XSL_HTML_STYLESHEET "${D_BRDIR}/brlcad-man-xhtml-stylesheet.xsl")
  set(XSL_MAN_STYLESHEET "${D_BRDIR}/brlcad-man-stylesheet.xsl")
  set(XSL_FO_STYLESHEET "${D_BRDIR}/brlcad-man-fo-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS)

# CTest Testing support (ON by default, may be controlled with BUILD_TESTING
# variable.)
include(CTest)
mark_as_advanced(BUILD_TESTING)

# Set up the BRL-CAD wrapper around find_package
include(BRLCAD_Find_Package)


###
# traverse into the source tree with intentional ordering
###

# sources before anything else so we can use tools built
verbose_add_subdirectory("" src)

# headers after sources - BRLCAD_COMPONENTS builds will install
# only a subset of headers, and that logic needs to know which
# library targets are active.
verbose_add_subdirectory("" include)

# scripts that primarily require rendering
if(TARGET rt)
  verbose_add_subdirectory("" sh)
  verbose_add_subdirectory("" regress)
  verbose_add_subdirectory("" bench)
endif(TARGET rt)

# require geometry conversion tools
if(TARGET asc2g)
  verbose_add_subdirectory("" db)
endif(TARGET asc2g)

# docs after we're all done with build tasks
# NOTE: doc/docbook/resources is added separately (earlier)
# NOTE: doc/doxygenis added separately (earlier)
verbose_add_subdirectory("" doc)

# add misc last
# NOTE: misc/tools is added separately (earlier)
verbose_add_subdirectory("" misc)


# Make sure we have the target dir
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/${DATA_DIR}/data")

# If we're building on Windows with MSVC, use config_win.h file
if(MSVC)
  config_h_append(BRLCAD "#include \"config_win.h\"\n")
endif(MSVC)

# Finalize brlcad_config.h
config_h_append(BRLCAD "#endif /* __CONFIG_H__ */\n")
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY ${CMAKE_PROJECT_NAME}_CONFIG_H_CONTENTS)
set_source_files_properties(${CONFIG_H_FILE} PROPERTIES GENERATED TRUE)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
build_cfg_hdr(${CONFIG_H_FILE} ${INCLUDE_DIR}/brlcad)

#---------------------------------------------------------------------
# Rules for the toplevel documentation files
set(
  toplevel_DOCFILES
  AUTHORS
  CHANGES
  COPYING
  HACKING
  INSTALL
  NEWS
  README
)
brlcad_adddata(toplevel_DOCFILES ".")

# Now that everything is configured, print a summary of the build
# settings.
if(NOT BRLCAD_DISABLE_SUMMARY)
  include(BRLCAD_Summary)
  brlcad_summary()
endif(NOT BRLCAD_DISABLE_SUMMARY)

# *******************************************************************
# ***                      Timestamp Rules                        ***
# *******************************************************************
# Set up rules to print a timestamp string during build
set(BUILD_DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_START")

add_custom_command(
  OUTPUT ${BUILD_DELTA_START}
  COMMAND "${CMAKE_COMMAND}" -DSTAMP_FILE=${BUILD_DELTA_START} -P "${BRLCAD_CMAKE_DIR}/scripts/timestamp.cmake"
  COMMENT ""
)
add_custom_target(
  timestamp
  ALL
  COMMAND "${CMAKE_COMMAND}" -DMSG=\"Build Time:\" -P "${BRLCAD_CMAKE_DIR}/scripts/printtime.cmake"
  DEPENDS ${BUILD_DELTA_START}
)
set_target_properties(timestamp PROPERTIES FOLDER "Compilation Utilities")
add_custom_target(
  buildtimedelta
  ALL
  COMMAND ${CMAKE_BINARY_DIR}/CMakeTmp/dreport${EXE_EXT} final ${BUILD_DELTA_START} ${CONFIG_DELTA_START}
  COMMAND ${CMAKE_COMMAND} -E remove ${BUILD_DELTA_START}
)
set_target_properties(buildtimedelta PROPERTIES FOLDER "Compilation Utilities")

#------------------------------------------------------------------------------
# We want the timestamp to come first, so make all targets depend on timestamp.
# Similarly, buildtimedelta needs to depend on every target not excluded from
# the default build list.  Doing this without function overrides for
# bookkeeping drives a minimum CMake version requirement of 3.7, in order to
# get the SUBDIRECTORIES and BUILDSYTEM_TARGETS properties.

# First, use the SUBDIRECTORIES property to build up a list of all
# active directories in CMake:
# https://cmake.org/cmake/help/latest/prop_dir/SUBDIRECTORIES.html
#
# We do this because we need to use get_property on them, and get_property will
# produce an error if we give it a non-CMake processed directory.  As a side
# benefit, it should also be considerably faster than using FILE_GLOB and
# trying to trim down the results.
set(ALL_DIRS)
get_property(SUBDIRS DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY SUBDIRECTORIES)
while(SUBDIRS)
  list(POP_FRONT SUBDIRS CDIR)
  set(ALL_DIRS ${ALL_DIRS} ${CDIR})
  get_property(CSUBDIRS DIRECTORY "${CDIR}" PROPERTY SUBDIRECTORIES)
  set(SUBDIRS ${SUBDIRS} ${CSUBDIRS})
endwhile(SUBDIRS)

# Next, for all active directories, collect the list of targets using the
# BUILDSYSTEM_TARGETS property set on each directory.

# Iterate over all the SUBDIRECTORIES identified directories to build up the
# comprehensive set.  As target names are unique, we don't need to worry about
# removing duplicates.  Because we're using the set of all active directories
# assembled above, this should collect all build targets we need to care about
# for timestamping purposes.
set(ALL_TARGETS)
foreach(ad ${ALL_DIRS})
  get_property(DIR_TARGETS DIRECTORY "${ad}" PROPERTY BUILDSYSTEM_TARGETS SET)
  if(DIR_TARGETS)
    get_property(DIR_TARGETS DIRECTORY "${ad}" PROPERTY BUILDSYSTEM_TARGETS)
    set(ALL_TARGETS ${ALL_TARGETS} ${DIR_TARGETS})
  endif(DIR_TARGETS)
endforeach(ad ${ALL_DIRS})

# Now, set up the target dependencies for timestamp and buildtimedelta.  These
# dependencies will produce a build-tool-independent ordering that gives us the
# timing behavior we want.
foreach(ctarget ${ALL_TARGETS})
  if(NOT ${ctarget} MATCHES "timestamp")
    add_dependencies(${ctarget} timestamp)
    if(NOT ${ctarget} MATCHES "managed_files" AND NOT ${ctarget} MATCHES ".*_cp$")
      add_dependencies(${ctarget} managed_files)
    endif()
  endif(NOT ${ctarget} MATCHES "timestamp")
  if(NOT ${ctarget} MATCHES "buildtimedelta")
    get_target_property(not_in_all ${ctarget} EXCLUDE_FROM_ALL)
    get_target_property(not_in_default ${ctarget} EXCLUDE_FROM_DEFAULT_BUILD)
    if(NOT not_in_all AND NOT not_in_default)
      add_dependencies(buildtimedelta ${ctarget})
    endif(NOT not_in_all AND NOT not_in_default)
  endif(NOT ${ctarget} MATCHES "buildtimedelta")
endforeach(ctarget ${ALL_TARGETS})

#------------------------------------------------------------------------------
# Use the set of all directories assembled above to also set up distclean rules.
# This eliminates the need to override add_subdirectory.
foreach(ad ${ALL_DIRS})
  get_property(BDIR DIRECTORY "${ad}" PROPERTY BINARY_DIR)
  distclean("${BDIR}/CMakeFiles")
  distclean("${BDIR}/cmake_install.cmake")
  foreach(clearpattern ${DISTCLEAN_OUTFILES})
    distclean("${BDIR}/${clearpattern}")
  endforeach(clearpattern ${DISTCLEAN_OUTFILES})
endforeach(ad ${ALL_DIRS})

#------------------------------------------------------------------------------
# CPack is used to produce tgz files, RPMS, etc.
#
# TODO - CPack's package_source makes lots of copies of files, which is a problem
# (or at least REALLY slows things down) if we want to include bext as part of the
# overall release tarballs.  May have to look into a custom solution to do the
# job more efficiently
include(BRLCAD_CPack)

# Some files to ignore for distcheck.  The files and directories prefixed with
# a "." character are "hidden" and may not be present, so check for them first.
set(
  hidden_files
  .github/workflows/analysis.yml
  .github/workflows/check.yml
  .github/workflows/linux_interactive.yml
  .gitattributes
  .gitignore
)
foreach(hf ${hidden_files})
  if(EXISTS ${CMAKE_SOURCE_DIR}/${hf})
    cmakefiles(${hf})
  endif(EXISTS ${CMAKE_SOURCE_DIR}/${hf})
endforeach(hf ${hidden_files})

set(
  toplevel_ignore_files
  BUGS
  CMakeLists.txt
  ChangeLog
  TODO
)
cmakefiles(${toplevel_ignore_files})

# Handle some toplevel distclean listings
distclean("${CMAKE_BINARY_DIR}/CMakeCache.txt")
distclean("${CMAKE_BINARY_DIR}/cmakefiles.cmake")
distclean("${CMAKE_BINARY_DIR}/brlcadexec.cmake")
distclean("${CMAKE_BINARY_DIR}/cmake_install.cmake")
distclean("${CMAKE_BINARY_DIR}/install_manifest.txt")
distclean("${CMAKE_BINARY_DIR}/CMakeFiles")
distclean("${CMAKE_BINARY_DIR}/CMakeTmp")
distclean("${CMAKE_BINARY_DIR}/include/brlcad_config.h.in")
distclean("${CMAKE_BINARY_DIR}/CMakeDoxyfile.in")
distclean("${CMAKE_BINARY_DIR}/CMakeDoxygenDefaults.cmake")
distclean("${CMAKE_BINARY_DIR}/CPackProperties.cmake")
foreach(clearpattern ${DISTCLEAN_OUTFILES})
  distclean("${CMAKE_BINARY_DIR}/${clearpattern}")
endforeach(clearpattern ${DISTCLEAN_OUTFILES})
if("${CMAKE_GENERATOR}" MATCHES "Ninja*")
  distclean("${CMAKE_BINARY_DIR}/.ninja_log")
  distclean("${CMAKE_BINARY_DIR}/.ninja_deps")
endif("${CMAKE_GENERATOR}" MATCHES "Ninja*")

# ----------------------------------------------------------------------------
# Define a distcheck target.  This performs a variety of tests to determine
# whether BRL-CAD is in a distribution ready state.  Default to the standard
# set of tests - Debug and Release build configurations
include(Distcheck)

# ----------------------------------------------------------------------------
# Mark various miscellaneous things as advanced that we don't want in our
# default view
mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY)
mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
mark_as_advanced(LIBRARY_OUTPUT_PATH)
mark_as_advanced(CMAKE_CXX_COMPILER)
mark_as_advanced(CMAKE_C_COMPILER)
mark_as_advanced(CMAKE_AR)

# ----------------------------------------------------------------------------
# Because the build-time-delta needs a configure-file but comes at the
# end of the CMake configure, the preparation of the final distclean
# list must account for it.  Also, the distclean script itself must
# be added to the list, as it is generated by CMake
distclean("${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c")
get_property(CMAKE_DISTCLEAN_TARGET_LIST GLOBAL PROPERTY CMAKE_DISTCLEAN_TARGET_LIST)
list(REMOVE_DUPLICATES CMAKE_DISTCLEAN_TARGET_LIST)
configure_file("${BRLCAD_CMAKE_DIR}/distclean.cmake.in" "${BRLCAD_BINARY_DIR}/distclean.cmake" @ONLY)
distclean("${BRLCAD_BINARY_DIR}/distclean.cmake")
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_custom_target(distclean COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake")
else("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_custom_target(
    distclean
    COMMAND ${CMAKE_COMMAND} -E echo "Running clean target..."
    COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E echo "Running clean target... done."
    COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake"
  )
endif("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
if(TARGET distclean)
  set_target_properties(distclean PROPERTIES FOLDER "Compilation Utilities")
endif(TARGET distclean)

# Have touched every file that we are going to touch with the build system -
# time to write out our list for future processing.
get_property(CMAKE_IGNORE_FILES GLOBAL PROPERTY CMAKE_IGNORE_FILES)
list(REMOVE_DUPLICATES CMAKE_IGNORE_FILES)
string(REPLACE ";" "\n" CMAKE_IGNORE_FILES "${CMAKE_IGNORE_FILES}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake" "${CMAKE_IGNORE_FILES}")

get_property(BRLCAD_EXEC_FILES GLOBAL PROPERTY BRLCAD_EXEC_FILES)
list(REMOVE_DUPLICATES BRLCAD_EXEC_FILES)
list(SORT BRLCAD_EXEC_FILES)
string(REPLACE ";" "\n" BRLCAD_EXEC_FILES "${BRLCAD_EXEC_FILES}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/brlcadexec.cmake" "${BRLCAD_EXEC_FILES}")

# Done with all really time-consuming steps - do the configure time delta
execute_process(COMMAND "${CMAKE_BINARY_DIR}/CMakeTmp/dreport" "Elapsed configuration time: " "${CONFIG_DELTA_START}")

# Write out the Doxygen feature list
doxygen_feature_summary("${BRLCAD_BINARY_DIR}/CMakeTmp/features.dox")

add_custom_target(
  print-warning-message
  ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \"test\" target runs all of BRL-CAD\\'s available unit tests"
  COMMAND ${CMAKE_COMMAND} -E echo "      including tests for API currently under development.  It is not"
  COMMAND ${CMAKE_COMMAND} -E echo "      necessarily cause for concern if any of these tests fail.  It is"
  COMMAND ${CMAKE_COMMAND} -E echo "      always helpful to fix tests that are failing, except this one."
  COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND false
)
set_target_properties(print-warning-message PROPERTIES FOLDER "Compilation Utilities")

add_test(
  NAME "NOTE: some 'test' tests are expected to fail, 'regress' must pass"
  COMMAND ${CMAKE_COMMAND} --build . --target print-warning-message
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
