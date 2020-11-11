#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2020 United States Government as represented by
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
# ******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(BRLCAD)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

# Test name character check - need to look into this one...
if (POLICY CMP0110)
  cmake_policy(SET CMP0110 OLD)
endif (POLICY CMP0110)

#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.  If we
# are a subbuild, let the parent's CMAKE_MODULE_PATH supply files before
# our own, otherwise misc/CMake takes first priority.
set(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
list(APPEND CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR}")


#---------------------------------------------------------------------
# Setup and checks related to system environment settings.  Some of
# these impact search results needed to set default options, so we
# do this early in the process.
include(BRLCAD_Environment_Setup)


#---------------------------------------------------------------------
# Define various utilities.
include(BRLCAD_Util)


#---------------------------------------------------------------------
# Define the current BRL-CAD version.
# See HACKING for details on how to properly update the version

file(READ "${BRLCAD_SOURCE_DIR}/include/conf/MAJOR" BRLCAD_VERSION_MAJOR)
string(STRIP ${BRLCAD_VERSION_MAJOR} BRLCAD_VERSION_MAJOR)
file(READ "${BRLCAD_SOURCE_DIR}/include/conf/MINOR" BRLCAD_VERSION_MINOR)
string(STRIP ${BRLCAD_VERSION_MINOR} BRLCAD_VERSION_MINOR)
file(READ "${BRLCAD_SOURCE_DIR}/include/conf/PATCH" BRLCAD_VERSION_PATCH)
string(STRIP ${BRLCAD_VERSION_PATCH} BRLCAD_VERSION_PATCH)

set(BRLCAD_VERSION "${BRLCAD_VERSION_MAJOR}.${BRLCAD_VERSION_MINOR}.${BRLCAD_VERSION_PATCH}")
if(DEFINED BRLCAD_VERSION_AMEND)
  set(BRLCAD_VERSION "${BRLCAD_VERSION}-${BRLCAD_VERSION_AMEND}")
endif(DEFINED BRLCAD_VERSION_AMEND)

#---------------------------------------------------------------------
# Let CMake know where to look for our counting file for configuration
# passes.  It will impact whether we print certain messages
set(BRLCAD_CNT_FILE "${BRLCAD_BINARY_DIR}/CMakeTmp/BRLCAD_BUILD_COUNT")
if(NOT EXISTS ${BRLCAD_CNT_FILE})
  set(BRLCAD_PRINT_MSGS 1)
else(NOT EXISTS ${BRLCAD_CNT_FILE})
  set(BRLCAD_PRINT_MSGS 0)
endif(NOT EXISTS ${BRLCAD_CNT_FILE})

#---------------------------------------------------------------------
# Define an option to use OBJECT libraries.  If we are building with object
# libraries, we need position independent code.
include(CMakeDependentOption)
cmake_dependent_option(USE_OBJECT_LIBS "Use OBJECT libraries" ON "NOT MSVC" OFF)
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
execute_process(COMMAND "${CMAKE_COMMAND}" -DSTAMP_FILE=${CONFIG_DELTA_START} -P "${BRLCAD_CMAKE_DIR}/scripts/timestamp.cmake")

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
# Management of build types
if (NOT BRLCAD_IS_SUBBUILD)
  include(BRLCAD_Build_Types)
endif (NOT BRLCAD_IS_SUBBUILD)

#---------------------------------------------------------------------
# For cleaning files as part of the distclean command, CMake needs
# to be aware of what various generators will (or might) write out
# in each build directory.
set(DISTCLEAN_OUTFILES
  CTestTestfile.cmake
  Testing/Temporary/CTestCostData.txt
  Testing/Temporary/LastTest.log
  )
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
if(NOT BRLCAD_IS_SUBBUILD)
  include(BRLCAD_Test_Wrappers)
endif(NOT BRLCAD_IS_SUBBUILD)

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
BOX_PRINT("*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***" "*")

#---------------------------------------------------------------------
# Set up include paths for generated header files.  For multi-config
# builds, make sure we get build-specific dirs.
if(CMAKE_CONFIGURATION_TYPES)
  include_directories(${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR})
  include_directories(${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/brlcad)
else(CMAKE_CONFIGURATION_TYPES)
  include_directories(${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad)
endif(CMAKE_CONFIGURATION_TYPES)

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it if it exists
if (IS_DIRECTORY /usr/local)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
  if (IS_DIRECTORY /usr/local/include)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)
  endif (IS_DIRECTORY /usr/local/include)
endif (IS_DIRECTORY /usr/local)

#---------------------------------------------------------------------
# Intricacies involved with setting the install path mostly revolve
# around build type dependent install directories.  Also needs the
# current version defined.

include(BRLCAD_Install_Prefix)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling
if (NOT COMMAND std_build_rpath)
  include(RPath_Setup)
endif (NOT COMMAND std_build_rpath)

# We want the full RPATH set in the build tree so we can run programs without
# needing to set LD_LIBRARY_PATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# We DON'T want the final install directory RPATH set in the build directory
# - it should only be set to the installation value when actually installed.
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Set RPATH value to use when installing.  This should be set to always
# prefer the version in the installed path when possible, but fall back on a
# location relative to the loading file's path if the installed version is
# not present.  How to do so is platform specific.
relative_rpath(RELPATH)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELPATH}")

std_build_rpath()


#---------------------------------------------------------------------
# We need compiler support for certain C and C++ standards when we
# build.  Set CMake's flags accordingly for what source code syntax
# (i.e., -std=xxx flags) and API (i.e., -D_POSIX_C_SOURCE defines) to
# permit and utilize respectively.  Common profiles:
#
#   2011-2016: C11 & C++11 (ignoring C++14, target: 2013)
#     ISO/IEC 9899:2011 __STDC_VERSION__==201112L
#     ISO/IEC 14882:2011 __cplusplus==201103L
#     IEEE 1003.1-2008 -D_POSIX_C_SOURCE=200809L
#     Open Group Single UNIX Specification, Version 4 (2008+) -D_XOPEN_SOURCE=700
#   2017-2020: C11 & C++17 (target: 2019)
#     ISO/IEC 9899:2011 __STDC_VERSION__==201112L
#     ISO/IEC 14882:2017 __cplusplus==201703L
#     IEEE 1003.1-2008 -D_POSIX_C_SOURCE=200809L
#     Open Group Single UNIX Specification, Version 4 (2008+) -D_XOPEN_SOURCE=700
#
# Sources: https://sourceforge.net/p/predef/wiki/Standards/

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
include(CheckCXXSourceRuns)
include(CMakePushCheckState)
function(BRLCAD_API_FLAG AFLAG AFLAGS)
  set(CHECK_API_FLAG_SRC "
#include <iostream>
int main(int ac, char *av[])
{
  if (ac > 0 && av)
    std::cout << \"hello\";
  return 0;
}
")
    string(TOUPPER "${AFLAG}" UAFLAG)
    string(REPLACE "-" "_" AFLAGVAR "${UAFLAG}")
    string(REPLACE "=" "_" AFLAGVAR "${AFLAGVAR}")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS} ${AFLAG}")
    check_cxx_source_runs("${CHECK_API_FLAG_SRC}" WORKING_${AFLAGVAR})
    cmake_pop_check_state()
    if(WORKING_${AFLAGVAR})
      set(${AFLAGS} "${${AFLAGS}} ${AFLAG}" PARENT_SCOPE)
    endif(WORKING_${AFLAGVAR})
endfunction(BRLCAD_API_FLAG)

set(API_FLAGS)
BRLCAD_API_FLAG("-D_POSIX_C_SOURCE=200809L" API_FLAGS)
BRLCAD_API_FLAG("-D_XOPEN_SOURCE=700" API_FLAGS)

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
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_STANDARD_FLAGS "${CMAKE_CXX_FLAGS_DEFAULT} ${CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION} ${API_FLAGS}")
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
# it is ready to process.  A global property is needed to hold the
# contents, because subdirectories (in particular, src/other) may
# have content to contribute to the top level config.h.in file and
# the default local variable scope in subdirectories means those
# changes would not automatically propagate back up.
#
# We also allow for multiple projects with this macro, in case
# subprojects are also managing a config.h.in a file of their own.

set(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")

set(CMAKE_CURRENT_PROJECT BRLCAD)

define_property(GLOBAL PROPERTY BRLCAD_CONFIG_H_CONTENTS BRIEF_DOCS "config.h.in contents" FULL_DOCS "config.h.in contents for BRL-CAD project")
if(NOT COMMAND CONFIG_H_APPEND)
  function(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endfunction(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)

CONFIG_H_APPEND(BRLCAD "/**** Define statements for CMake ****/\n")
CONFIG_H_APPEND(BRLCAD "#if !defined(BRLCADBUILD)\n")
CONFIG_H_APPEND(BRLCAD "  #  pragma message \"Warning: included brlcad_config.h (compile-time API) without BRLCADBUILD defined\"\n")
CONFIG_H_APPEND(BRLCAD "#endif\n")
CONFIG_H_APPEND(BRLCAD "#if !defined(HAVE_CONFIG_H)\n")
CONFIG_H_APPEND(BRLCAD "  #  pragma message \"Warning: included brlcad_config.h (compile-time API) without HAVE_CONFIG_H defined\"\n")
CONFIG_H_APPEND(BRLCAD "#endif\n")
CONFIG_H_APPEND(BRLCAD "#ifndef __CONFIG_H__\n")
CONFIG_H_APPEND(BRLCAD "#define __CONFIG_H__\n")

# Set up some of the define statements for path information and other basics
CONFIG_H_APPEND(BRLCAD "#define PACKAGE \"brlcad\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_BUGREPORT \"https://brlcad.org\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_NAME \"BRL-CAD\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_STRING \"BRL-CAD ${BRLCAD_VERSION}\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_TARNAME \"brlcad\"\n")

# Let programs know what the executable suffix and lib suffix are on this platform, if any
CONFIG_H_APPEND(BRLCAD "#define EXECUTABLE_SUFFIX \"${CMAKE_EXECUTABLE_SUFFIX}\"\n")
CONFIG_H_APPEND(BRLCAD "#define SHARED_LIBRARY_SUFFIX \"${CMAKE_SHARED_LIBRARY_SUFFIX}\"\n")

# Let bu_dir know what the target install directory is.  In a superbuild
# configuration this will be the most import place to be sure that we get the
# actual superbuild install path, not the local BRL-CAD subbuild install path.
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_ROOT \"${CMAKE_INSTALL_PREFIX}\"\n")

# Define the various relative paths for bu_dir (be sure to have included
# Path_Setup.cmake before this point, as that file defines these variables.)
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_BIN_DIR \"${BIN_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_LIB_DIR \"${LIB_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_LIBEXEC_DIR \"${LIBEXEC_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_INCLUDE_DIR \"${INCLUDE_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DATA_DIR \"${DATA_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DOC_DIR \"${DOC_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_MAN_DIR \"${MAN_DIR}\"\n")

# Define the ammendment count if we have one
if(DEFINED BRLCAD_VERSION_AMEND)
  CONFIG_H_APPEND(BRLCAD "#define BRLCAD_VERSION_AMEND ${BRLCAD_VERSION_AMEND}\n")
endif(DEFINED BRLCAD_VERSION_AMEND)

#----------------------------------------------------------------------
# If we're not debugging, we want HIDDEN to be defined as static.  Set
# the NDEBUG flag accordingly, and common.h will take it from there.
# Don't set this if we're doing multi-config, since whether we want
# this on is not ultimately decided at configure time.
if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT CMAKE_CONFIGURATION_TYPES)
  CONFIG_H_APPEND(BRLCAD "#define NDEBUG 1\n")
endif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT CMAKE_CONFIGURATION_TYPES)

#----------------------------------------------------------------------
# Let config.h know whether we're doing a 32 or a 64 bit build.

CONFIG_H_APPEND(BRLCAD "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")

# OpenBSD doesn't define __WORD_SIZE
if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  CONFIG_H_APPEND(BRLCAD "#ifndef __WORDSIZE\n#  define __WORDSIZE 32\n#endif\n")
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")
if(${CMAKE_WORD_SIZE} MATCHES "64BIT")
  CONFIG_H_APPEND(BRLCAD "#ifndef __WORDSIZE\n#  define __WORDSIZE 64\n#endif\n")
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT")

# Auto-reconfiguration - by default, a CMake generated build system
# will re-run CMake if it detects that build system logic has changed.
# This is normally a good thing, but becomes problematic when using
# Visual Studio - recent versions of MSVC will individually prompt for
# a re-loading of generated solution files one at a time.  Since
# BRL-CAD has over a thousand such files in a default build, the only
# viable approach is to close Visual Studio, re-run CMake manually,
# and re-open the project in Visual Studio.
if("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(CMAKE_SUPPRESS_REGENERATION ON)
endif("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*        Stage 1 of 9 - Top Level Configure Options       *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# The BRL-CAD CMake build will also generate a configure script
# that emulates the command option style of GNU Autotool's
# configure.  Write the pre-defined header into the build-dir template
# to initialize the file.
file(REMOVE "${CMAKE_BINARY_DIR}/configure.new")
file(READ "${BRLCAD_CMAKE_DIR}/configure_prefix.sh" CONFIG_PREFIX)
file(WRITE "${CMAKE_BINARY_DIR}/configure.new.tmp" "${CONFIG_PREFIX}")
file(COPY "${CMAKE_BINARY_DIR}/configure.new.tmp" DESTINATION
  "${CMAKE_BINARY_DIR}/CMakeFiles" FILE_PERMISSIONS OWNER_READ OWNER_WRITE
  OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(REMOVE "${CMAKE_BINARY_DIR}/configure.new.tmp")
file(RENAME "${CMAKE_BINARY_DIR}/CMakeFiles/configure.new.tmp"
  "${CMAKE_BINARY_DIR}/configure.new")

include(BRLCAD_User_Options)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*             Stage 2 of 9 - Check for Programs           *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# Load various wrapper macros for checking libraries, headers and
# functions, some in use by src/other build logic
include(BRLCAD_CheckFunctions)

# Load Doxygen related CMake macros
include(Doxygen)

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
find_package(Doxygen)


# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*     Stage 3 of 9 - Check for Compiler Characteristics   *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# TODO - the Ninja single config generator in Release build uses the O3
# and other optimization flags, but they don't seem to make it to the
# step-g sources in MultiConfig release build???

# load our compiler testing macro definitions
include(CompilerFlags)

# Cache the original CMake sets of build flags for later use
if(NOT CMAKE_BUILD_FLAGS_CACHED_CMAKE_DEFAULT)
  CACHE_BUILD_FLAGS(_CMAKE_DEFAULT)
endif(NOT CMAKE_BUILD_FLAGS_CACHED_CMAKE_DEFAULT)

# Clear out most CMake-assigned defaults - We're managing
# our own compile flags, and don't (for example) want NDEBUG
# if we have debugging flags enabled for a Release build.
# At the same time, pull in any flags that have been set
# in the environment.
CLEAR_BUILD_FLAGS()

# Since this isn't src/other or misc/tools, we want the standards set.
set(CMAKE_C_FLAGS "${C_STANDARD_FLAGS}")
set(CMAKE_CXX_FLAGS "${CXX_STANDARD_FLAGS}")

# try to use -pipe to speed up the compiles
CHECK_C_FLAG(pipe)
CHECK_CXX_FLAG(pipe)

# Enable visibility restrictions.  We have to deal with this on Windows, so
# enable it wherever we can to keep the code working across all platforms.
CHECK_C_FLAG(fvisibility=hidden)
CHECK_CXX_FLAG(fvisibility=hidden)
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
CHECK_C_FLAG(fno-strict-aliasing)
CHECK_CXX_FLAG(fno-strict-aliasing)

# check for -fno-common (libtcl needs it on darwin)
CHECK_C_FLAG(fno-common)
CHECK_CXX_FLAG(fno-common)

# check for -fexceptions
# this is needed to resolve __Unwind_Resume when compiling and
# linking against openNURBS in librt for any -static binaries
CHECK_C_FLAG(fexceptions)
CHECK_CXX_FLAG(fexceptions)

# check for -ftemplate-depth-NN this is needed in libpc and
# other code using boost where the template instantiation depth
# needs to be increased from the default ANSI minimum of 17.
CHECK_CXX_FLAG(ftemplate-depth-128)

# check for llvm libFuzzer support
CHECK_CXX_FLAG(fsanitize=fuzzer)

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
  CHECK_C_FLAG(m64 VARS 64BIT_FLAG)
  CHECK_C_FLAG("arch x86_64" VARS 64BIT_FLAG)
  CHECK_C_FLAG(64 VARS 64BIT_FLAG)
  CHECK_C_FLAG("mabi=64" VARS  64BIT_FLAG)
  if(NOT 64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
    message(FATAL_ERROR "Trying to compile 64BIT but all 64 bit compiler flag tests failed!")
  endif(NOT 64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
  CHECK_C_FLAG(q64 VARS 64BIT_FLAG)
  ADD_NEW_FLAG(C 64BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 64BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 64BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 64BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64)

# 32 bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO" AND NOT MSVC)
  CHECK_C_FLAG(m32 VARS 32BIT_FLAG)
  CHECK_C_FLAG("arch i686" VARS 32BIT_FLAG)
  CHECK_C_FLAG(32 VARS 32BIT_FLAG)
  CHECK_C_FLAG("mabi=32" VARS 32BIT_FLAG)
  CHECK_C_FLAG(q32 VARS 32BIT_FLAG)
  if(NOT 32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
    message(FATAL_ERROR "Trying to compile 32BIT but all 32 bit compiler flag tests failed!")
  endif(NOT 32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
  ADD_NEW_FLAG(C 32BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 32BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 32BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 32BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO" AND NOT MSVC)

# Debugging flags
if(BRLCAD_FLAGS_DEBUG)
  CHECK_C_FLAG(g GROUPS DEBUG_C_FLAGS)
  CHECK_CXX_FLAG(g GROUPS DEBUG_CXX_FLAGS)
  if(APPLE)
    EXEC_PROGRAM(sw_vers ARGS -productVersion OUTPUT_VARIABLE MACOSX_VERSION)
    if(${MACOSX_VERSION} VERSION_LESS "10.5")
      CHECK_C_FLAG(ggdb3 GROUPS DEBUG_C_FLAGS)
      CHECK_CXX_FLAG(ggdb3 GROUPS DEBUG_CXX_FLAGS)
    else(${MACOSX_VERSION} VERSION_LESS "10.5")
      # CHECK_C_COMPILER_FLAG silently eats gstabs+ - also, compiler
      # apparently doesn't like mixing stabs with another debug flag.
      set(DEBUG_C_FLAGS "-ggdb")
      set(DEBUG_CXX_FLAGS "-ggdb")
    endif(${MACOSX_VERSION} VERSION_LESS "10.5")
  else(APPLE)
    CHECK_C_FLAG(ggdb3 GROUPS DEBUG_C_FLAGS)
    CHECK_CXX_FLAG(ggdb3 GROUPS DEBUG_CXX_FLAGS)
  endif(APPLE)
  if(CMAKE_CONFIGURATION_TYPES)
    set(debug_config_list "${CMAKE_CONFIGURATION_TYPES}")
  else(CMAKE_CONFIGURATION_TYPES)
    set(debug_config_list "ALL")
  endif(CMAKE_CONFIGURATION_TYPES)
  ADD_NEW_FLAG(C DEBUG_C_FLAGS "${debug_config_list}")
  ADD_NEW_FLAG(CXX DEBUG_CXX_FLAGS "${debug_config_list}")
  # TODO - need to figure out a way to actually test linker flags
  ADD_NEW_FLAG(SHARED_LINKER DEBUG_C_FLAGS "${debug_config_list}")
  ADD_NEW_FLAG(EXE_LINKER DEBUG_C_FLAGS "${debug_config_list}")
  mark_as_advanced(DEBUG_FLAGS)
endif(BRLCAD_FLAGS_DEBUG)


# Warn if no minimum compilation linkage is set for Mac systems
if(APPLE AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
  message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nMACOSX_DEPLOYMENT_TARGET should be set for release builds\nto something less than the latest release for portability\ne.g., export MACOSX_DEPLOYMENT_TARGET=10.5\nSee https://github.com/brlcad/MacOSX-SDKs\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
endif(APPLE AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")


# Most of the src/other projects have their own logic for handling
# the C inline issue - BRL-CAD needs a fine-grained approach.  Mixed
# C and C++ sources require different treatment for the same build
# target, since C++11 doesn't allow inline to be re-defined. See
# misc/CMake/BRLCAD_Targets.cmake for handling of C_INLINE.
CHECK_C_INLINE(C_INLINE)
if(NOT HAVE_INLINE_KEYWORD AND HAVE___INLINE_KEYWORD)
  CONFIG_H_APPEND(BRLCAD "#ifndef __cplusplus\n")
  CONFIG_H_APPEND(BRLCAD "#  define inline __inline\n")
  CONFIG_H_APPEND(BRLCAD "#endif /* !__cplusplus */\n")
elseif(HAVE_INLINE_KEYWORD)
  # this ugly hack is to avoid broken Mac OS X (10.13 era) ctype
  # headers that drop the inline keyword when compiling c99.
  CONFIG_H_APPEND(BRLCAD "#if !defined(inline) && !defined(__cplusplus)\n")
  CONFIG_H_APPEND(BRLCAD "#  define inline inline\n")
  CONFIG_H_APPEND(BRLCAD "#endif /* !inline && !__cplusplus */\n")
endif(NOT HAVE_INLINE_KEYWORD AND HAVE___INLINE_KEYWORD)

# If doing an optimized build, set _FORTIFY_SOURCE to 2.  Provides
# compile-time best-practice error checking on certain libc functions
# (e.g., memcpy), and provides run-time checks on buffer lengths and
# memory regions.  Unfortunately, glibc-1.6 made _FORTIFY_SOURCE spew
# an unquellable warning if optimization is disabled so we can't tie
# the flag to debug builds.
if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  CONFIG_H_APPEND(BRLCAD "#ifndef _FORTIFY_SOURCE\n#  define _FORTIFY_SOURCE 2\n#endif\n")
endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")

# Enable this flag for additional reporting of undefined symbols.
# TODO: Fixing these is a work in progress.
# CHECK_C_COMPILER_FLAG("Wl,--no-undefined" NO_UNDEFINED_LINKER_FLAG)

# ******************************************************************* #
# For some tests, we need Werror to make sure the test actually fails

# TODO - replace this cache approach with the CMakePushCheckState module
set(CMAKE_REQUIRED_FLAGS_BAK ${CMAKE_REQUIRED_FLAGS})
CHECK_C_FLAG(Werror VARS ERROR_FLAG)
if (ERROR_FLAG)
  set(CMAKE_REQUIRED_FLAGS "-Werror")
endif (ERROR_FLAG)

# Check whether the compiler supports __attribute__((format (__printf__, 1, 2)))
check_c_source_compiles("int pf(const char *f, ...) __attribute__((format (__printf__, 1, 2))); int pf(const char *f, ...){return 1;} int main(int argc, char *argv[]) {return pf(\"%c\",'a');}" HAVE_PRINTF12_ATTRIBUTE)
if(HAVE_PRINTF12_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_PRINTF12_ATTRIBUTE 1\n")
endif(HAVE_PRINTF12_ATTRIBUTE)

# Check whether the compiler supports __attribute__((format (__printf__, 2, 3)))
check_c_source_compiles("int pf(void *o, const char *f, ...) __attribute__((format (__printf__, 2, 3))); int pf(void *o, const char *f, ...){return 1;} int main(int argc, char *argv[]) {return pf((void *)0, \"%c\",'a');}" HAVE_PRINTF23_ATTRIBUTE)
if(HAVE_PRINTF23_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_PRINTF23_ATTRIBUTE 1\n")
endif(HAVE_PRINTF23_ATTRIBUTE)

# Check whether the compiler supports __attribute__((format (__scanf__, 2, 3)))
check_c_source_compiles("int sf(void *o, const char *f, ...) __attribute__((format (__scanf__, 2, 3))); int sf(void *o, const char *f, ...){return 1;} int main(int argc, char *argv[]) {int i = 1; return sf((void *)0, \"%d\", &i);}" HAVE_SCANF23_ATTRIBUTE)
if(HAVE_SCANF23_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_SCANF23_ATTRIBUTE 1\n")
endif(HAVE_SCANF23_ATTRIBUTE)

include(CheckCSourceCompiles)
# Check whether the compiler supports __attribute__((__noreturn__))
check_c_source_compiles("#include <stdlib.h>\n
void noret() __attribute__((__noreturn__)); void noret(){exit(1);} int main(int argc, char *argv[]) {noret(); return 0;}" HAVE_NORETURN_ATTRIBUTE)
if(HAVE_NORETURN_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_NORETURN_ATTRIBUTE 1\n")
endif(HAVE_NORETURN_ATTRIBUTE)

# Check whether the compiler supports __attribute__((analyzer_noreturn))
check_c_source_compiles("void anoret() __attribute__((analyzer_noreturn)); void anoret(){return;} int main(int argc, char *argv[]) {anoret(); return 0;}" HAVE_ANALYZER_NORETURN_ATTRIBUTE)
if(HAVE_ANALYZER_NORETURN_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_ANALYZER_NORETURN_ATTRIBUTE 1\n")
endif(HAVE_ANALYZER_NORETURN_ATTRIBUTE)

# Check whether the compiler supports __attribute__((always_inline))
check_c_source_compiles("inline void always_inline() __attribute__((always_inline)); inline void always_inline(){return;} int main(int argc, char *argv[]) {always_inline(); return 0;}" HAVE_ALWAYS_INLINE_ATTRIBUTE)
if(HAVE_ALWAYS_INLINE_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_ALWAYS_INLINE_ATTRIBUTE 1\n")
endif(HAVE_ALWAYS_INLINE_ATTRIBUTE)

# Check whether the compiler supports __attribute__((const))
check_c_source_compiles("void func() __attribute__((const)); void func(){return;} int main(int argc, char *argv[]) {func(); return 0;}" HAVE_CONST_ATTRIBUTE)
if(HAVE_CONST_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_CONST_ATTRIBUTE 1\n")
endif(HAVE_CONST_ATTRIBUTE)

# Check whether the compiler supports __attribute__((pure))
check_c_source_compiles("void func() __attribute__((pure)); void func(){return;} int main(int argc, char *argv[]) {func(); return 0;}" HAVE_PURE_ATTRIBUTE)
if(HAVE_PURE_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_PURE_ATTRIBUTE 1\n")
endif(HAVE_PURE_ATTRIBUTE)

# Check whether the compiler supports __attribute__((cold))
check_c_source_compiles("void func() __attribute__((cold)); void func(){return;} int main(int argc, char *argv[]) {func(); return 0;}" HAVE_COLD_ATTRIBUTE)
if(HAVE_COLD_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_COLD_ATTRIBUTE 1\n")
endif(HAVE_COLD_ATTRIBUTE)

# Check whether the compiler supports __attribute__((nonnull))
check_c_source_compiles("int *func(int *) __attribute__((nonnull)); int *func(int *v){(*v)-=1; return v;} int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}" HAVE_NONNULL_ATTRIBUTE)
if(HAVE_NONNULL_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_NONNULL_ATTRIBUTE 1\n")
endif(HAVE_NONNULL_ATTRIBUTE)

# Check whether the compiler supports __attribute__((warn_unused_result))
check_c_source_compiles("int *func(int *) __attribute__((warn_unused_result)); int *func(int *v){(*v)-=1; return v;} int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}" HAVE_WARN_UNUSED_RESULT_ATTRIBUTE)
if(HAVE_WARN_UNUSED_RESULT_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_WARN_UNUSED_RESULT_ATTRIBUTE 1\n")
endif(HAVE_WARN_UNUSED_RESULT_ATTRIBUTE)

# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care.
CHECK_C_FLAG(Qunused-arguments)
CHECK_CXX_FLAG(Qunused-arguments)


set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_BAK})


# The following tests are almost entirely for gcc/llvm style flags, so
# we don't unnecessarily slow down the Windows build.
if(NOT MSVC)

  # This is nominally a header test, but because it can impact the C/C++ flags
  # we do it before the final caching of BRL-CAD flags.  We need this because
  # pthread headers on some BSD platforms still haven't been scrubbed for
  # c90/posix.1 compliance - see r70785
  BRLCAD_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H)
  if (NOT PROBE_PTHREAD_H)
    cmake_push_check_state(RESET)
    # pthread.h on FreeBSD 10 and some older Linucies use non-c90 types
    set(CMAKE_REQUIRED_DEFINITIONS "-Dclockid_t=clock_t")
    set(CMAKE_REQUIRED_FLAGS "-pthread")
    BRLCAD_INCLUDE_FILE(pthread.h PROBE_PTHREAD_H_CLOCKID_T)
    if (PROBE_PTHREAD_H_CLOCKID_T)
      set(C_STANDARD_FLAGS "${C_STANDARD_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CXX_STANDARD_FLAGS "${CXX_STANDARD_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dclockid_t=clock_t -pthread")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dclockid_t=clock_t -pthread")
      set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    endif (PROBE_PTHREAD_H_CLOCKID_T)
  endif (NOT PROBE_PTHREAD_H)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

  # -O3/-Ofast can enable -ffast-math which can provoke a stack
  # corruption in the shadow computations because of strict aliasing
  # getting enabled.  we _require_ -fno-strict-aliasing until someone
  # changes how lists are managed.  -fast-math results in non-IEEE
  # floating point math among a handful of other optimizations that
  # cause substantial error in ray tracing and tessellation (and
  # probably more).

  CHECK_C_FLAG(O3 GROUPS OPTIMIZE_C_FLAGS)
  CHECK_CXX_FLAG(O3 GROUPS OPTIMIZE_CXX_FLAGS)

  # goes a step beyond O3, possibly using dangerous imprecise math
  # CHECK_C_FLAG(Ofast GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG(Ofast GROUPS OPTIMIZE_CXX_FLAGS)

  # binaries will not necessarily work on a different machine
  # CHECK_C_FLAG(march=native GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG(march=native GROUPS OPTIMIZE_CXX_FLAGS)

  # perform interprocedural analysis, slows compilation heavily
  CHECK_C_FLAG(fipa-pta GROUPS OPTIMIZE_C_FLAGS)
  CHECK_CXX_FLAG(fipa-pta GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O2, but not in older compilers
  CHECK_C_FLAG(fstrength-reduce GROUPS OPTIMIZE_C_FLAGS)
  CHECK_CXX_FLAG(fstrength-reduce GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O2, but not in older compilers
  CHECK_C_FLAG(fexpensive-optimizations GROUPS OPTIMIZE_C_FLAGS)
  CHECK_CXX_FLAG(fexpensive-optimizations GROUPS OPTIMIZE_CXX_FLAGS)

  # enabled at -O3, but not in older compilers
  CHECK_C_FLAG(finline-functions GROUPS OPTIMIZE_C_FLAGS)
  CHECK_CXX_FLAG(finline-functions GROUPS OPTIMIZE_CXX_FLAGS)

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
      message(FATAL_ERROR "PGO path ${BRLCAD_PGO_PATH} does not exist, but C flags contain the -fprofile-use flag.  This probably means you need to remove a stale CMakeCache.txt file to clear the build flags before performing the first pass of the PGO compilation.\n")
    endif("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-use.*")

    # gcc-style GEN
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
        set(flags "fprofile-generate=\"${BRLCAD_PGO_PATH}\" -Wno-error=coverage-mismatch -fprofile-correction")
        set(CMAKE_REQUIRED_FLAGS "-${flags}")
        CHECK_C_FLAG("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
        CHECK_CXX_FLAG("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    # llvm-style GEN
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
        set(flags "fprofile-generate=\"${BRLCAD_PGO_PATH}\"")
        set(CMAKE_REQUIRED_FLAGS "-${flags}")
        CHECK_C_FLAG("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
        CHECK_CXX_FLAG("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    if(NOT "${pgo_flags_found}" STREQUAL "")
      DISTCLEAN(${BRLCAD_PGO_PATH})
    endif(NOT "${pgo_flags_found}" STREQUAL "")

  elseif(BRLCAD_PGO AND EXISTS "${BRLCAD_PGO_PATH}")

    if(CMAKE_C_FLAGS)
      if("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-generate.*")
	message(FATAL_ERROR "PGO path ${BRLCAD_PGO_PATH} exists, but C flags contain the -fprofile-generate flag.  This probably means you need to remove a stale CMakeCache.txt file to clear the build flags before performing the second pass of the PGO compilation.\n")
      endif("${CMAKE_C_FLAGS}" MATCHES ".*fprofile-generate.*")
    endif(CMAKE_C_FLAGS)

    # gcc-style USE
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
        set(flags "fprofile-use=\"${BRLCAD_PGO_PATH}\" -Wno-error=coverage-mismatch -fprofile-correction")
        set(CMAKE_REQUIRED_FLAGS "-${flags}")
        CHECK_C_FLAG("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
        CHECK_CXX_FLAG("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")

    # llvm-style USE
    if("${pgo_flags_found}" STREQUAL "")
      cmake_push_check_state()
        set(flags "fprofile-use=\"${BRLCAD_PGO_PATH}\" -Wno-profile-instr-out-of-date -Wno-profile-instr-unprofiled")
        set(CMAKE_REQUIRED_FLAGS "-${flags}")
        CHECK_C_FLAG("${flags}" GROUPS OPTIMIZE_C_FLAGS VARS pgo_flags_found)
        CHECK_CXX_FLAG("${flags}" GROUPS OPTIMIZE_CXX_FLAGS)
      cmake_pop_check_state()
    endif("${pgo_flags_found}" STREQUAL "")
  endif(BRLCAD_PGO AND NOT EXISTS "${BRLCAD_PGO_PATH}")

  # CHECK_C_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_C_FLAGS)
  # CHECK_CXX_FLAG("finline-limit=10000 --param inline-unit-growth=300 --param large-function-growth=300" GROUPS OPTIMIZE_CXX_FLAGS)

  # link-time optimization (LTO) results in slower linking, more
  # warnings, and better optimization, but requires newer versions of
  # compilers to avoid common bugs.
  if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)
    # LTO bug in GCC 4.8.5 (__fread_chk_warn)
  else ()
    CHECK_C_FLAG(flto GROUPS OPTIMIZE_C_FLAGS)
    CHECK_CXX_FLAG(flto GROUPS OPTIMIZE_CXX_FLAGS)

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

  endif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)


  # GCC 4.9+ introduced "thin" LTO object files that require special
  # ar/ranlib handling via wrappers or plugin specification.  These
  # wrappers work fine even if we're not using LTO, so just use them
  # if found.
  if (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.8)
    find_program(GCC_AR_LTO ${CMAKE_C_COMPILER}-ar NAMES gcc-ar DOC "GNU ARchiver wrapper with LTO support")
    if (GCC_AR_LTO)
      SET(CMAKE_AR "${GCC_AR_LTO}" CACHE FILEPATH "Archiver" FORCE)
    endif (GCC_AR_LTO)
    find_program(GCC_RANLIB_LTO ${CMAKE_C_COMPILER}-ranlib NAMES gcc-ranlib DOC "GNU Ranlib wrapper with LTO support")
    if (GCC_RANLIB_LTO)
      SET(CMAKE_RANLIB "${GCC_RANLIB_LTO}" CACHE FILEPATH "Ranlib" FORCE)
    endif (GCC_RANLIB_LTO)
  endif (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.8)

  # only omit the frame pointer if we have no interest in profiling or
  # debugging.

  if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
    if(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
      CHECK_C_FLAG(fomit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
      CHECK_CXX_FLAG(fomit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
    else(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
      CHECK_C_FLAG(fno-omit-frame-pointer GROUPS OPTIMIZE_C_FLAGS)
      CHECK_CXX_FLAG(fno-omit-frame-pointer GROUPS OPTIMIZE_CXX_FLAGS)
    endif(NOT BRLCAD_ENABLE_PROFILING AND NOT BRLCAD_FLAGS_DEBUG)
  endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")

  # add optimization flags to either all configurations, if enabled,
  # or just Release builds.
  if(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
    if(CMAKE_CONFIGURATION_TYPES)
      if(NOT ENABLE_ALL_CONFIG_TYPES)
	set(opt_conf_list "Release")
      else(NOT ENABLE_ALL_CONFIG_TYPES)
	set(opt_conf_list "Release;RelWithDebInfo;MinSizeRel")
      endif(NOT ENABLE_ALL_CONFIG_TYPES)
    else(CMAKE_CONFIGURATION_TYPES)
      if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
	set(opt_conf_list "ALL")
      endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
    endif(CMAKE_CONFIGURATION_TYPES)
  else(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
    if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
      set(opt_conf_list "ALL")
    endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "ON")
  endif(${BRLCAD_FLAGS_OPTIMIZATION} MATCHES "AUTO")
  if(opt_conf_list)
    ADD_NEW_FLAG(C OPTIMIZE_C_FLAGS "${opt_conf_list}")
    ADD_NEW_FLAG(CXX OPTIMIZE_CXX_FLAGS "${opt_conf_list}")
  endif(opt_conf_list)

  # enable stack protection for unoptimized debug builds.  this is
  # intended to help make it easier to identify problematic code but
  # only when compiling unoptimized (because the extra barrier checks
  # can affect the memory footprint and runtime performance.
  if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)
    CHECK_C_FLAG(fstack-protector-all)
    CHECK_CXX_FLAG(fstack-protector-all)
    # checking both in case compiling c/c++ with different compilers
    CHECK_C_FLAG(qstackprotect)
    CHECK_CXX_FLAG(qstackprotect)
  endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)

  # Similarly, enable the AddressSanitizer memory address sanitizer.
  # See https://code.google.com/p/address-sanitizer/ for more info.
  # This typically is reported to cause a 2x slowdown.
  if(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)
    CHECK_C_FLAG(fsanitize=address)
    CHECK_C_FLAG(fno-omit-frame-pointer)
    CHECK_CXX_FLAG(fsanitize=address)
    CHECK_CXX_FLAG(fno-omit-frame-pointer)
  endif(${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)

  # Differently, enable the ThreadSanitizer race condition detector
  # only as requested since it can incur a 5x-15x slowdown.  See
  # https://code.google.com/p/thread-sanitizer/ for more info.
  if(BRLCAD_SANITIZE_THREAD)
    CHECK_C_FLAG(fsanitize=thread)
    CHECK_CXX_FLAG(fsanitize=thread)
  endif(BRLCAD_SANITIZE_THREAD)

  # verbose warning flags.  we intentionally try to turn on as many as
  # possible.  adding more is encouraged (as long as all issues are
  # fixed first).
  if(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)
    # also of interest:
    # -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
    # -Wformat=2 (after bu_fopen_uniq() is obsolete)
    CHECK_C_FLAG(pedantic)
    CHECK_CXX_FLAG(pedantic)

    #Added to make llvm happy on FreeBSD, but a good idea anyway
    CHECK_C_FLAG(pedantic-errors)
    #CHECK_CXX_FLAG(pedantic-errors)  #This line makes poissonrecon break, so disable for now

    # this catches a lot, it's good
    CHECK_C_FLAG(Wall)
    CHECK_CXX_FLAG(Wall)

    # this catches a lot more, also good
    CHECK_C_FLAG(Wextra)
    CHECK_CXX_FLAG(Wextra)

    # make sure our preprocessor logic references defined symbol names
    CHECK_C_FLAG(Wundef)
    CHECK_CXX_FLAG(Wundef)

    # this makes sure we don't try to compare floating point exactly
    CHECK_C_FLAG(Wfloat-equal)
    CHECK_CXX_FLAG(Wfloat-equal)

    # make sure we're using unambiguous symbol names, no shadowing
    CHECK_C_FLAG(Wshadow)
    CHECK_CXX_FLAG(Wshadow)

    # make sure we're not dangerously casting return types. C-only for
    # gcc, but maybe not for clang or others.
    CHECK_C_FLAG(Wbad-function-cast)
    CHECK_CXX_FLAG(Wbad-function-cast)

    # C-only: this makes sure C sources will compile as C++ code
    CHECK_C_FLAG(Wc++-compat)

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
    CHECK_C_FLAG(Winline)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      # g++ 4.7 spews unquellable bogus warnings on default
      # constructors (e.g., in opennurbs and boost) and there
      # are reports of problems with newer GCC as well.
      CHECK_CXX_FLAG(Wno-inline)
    else()
      CHECK_CXX_FLAG(Winline)
    endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # NOTE: Cannot directly test for -Wno-... flags because GCC
    # intentionally emits no diagnostic, only warning about
    # unrecognized options when other diagnostics are produced.
    # Instead, we must use an indirect test via -Wno-error=...

    CHECK_C_FLAG(Wno-error=long-long VARS long_long)
    if(long_long)
      # this makes sure we don't warn about using long long.  really, it's okay.
      CHECK_C_FLAG(Wno-long-long)
      CHECK_CXX_FLAG(Wno-long-long)
    endif(long_long)

    CHECK_C_FLAG(Wno-error=variadic-macros VARS variadic_macros)
    if(variadic_macros)
      # this is for X11 headers, they use variadic macros
      CHECK_C_FLAG(Wno-variadic-macros)
      CHECK_CXX_FLAG(Wno-variadic-macros)
    endif(variadic_macros)

    # this is used to check Doxygen comment syntax
    CHECK_C_FLAG(Wdocumentation)
    CHECK_CXX_FLAG(Wdocumentation)

  endif(BRLCAD_ENABLE_COMPILER_WARNINGS OR BRLCAD_ENABLE_STRICT)

  if(BRLCAD_ENABLE_COVERAGE)
    # TODO: These seem to GCC specific flags - should probably set up for clang
    # as well if that's our compiler...
    # https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
    # If we need different settings for different tools, should
    # encapsulate each tool's setup in a function
    CHECK_C_FLAG(ftest-coverage)
    CHECK_CXX_FLAG(ftest-coverage)
    if(FTEST_COVERAGE_C_FLAG_FOUND)
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage -fprofile-arcs")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage -fprofile-arcs")
      SET(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -lgcov -ftest-coverage -fprofile-arcs")
      SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lgcov -ftest-coverage -fprofile-arcs")
    else(FTEST_COVERAGE_C_FLAG_FOUND)
      message(SEND_ERROR "Building with coverage is not supported by BRL-CAD on this platform.")
    endif(FTEST_COVERAGE_C_FLAG_FOUND)
  endif(BRLCAD_ENABLE_COVERAGE)

  if(BRLCAD_ENABLE_STRICT)

    # NOTE: Cannot directly test for -Wno-... flags because GCC
    # intentionally emits no diagnostic, only warning about
    # unrecognized options when other diagnostics are produced.
    # Instead, we must use an indirect test via -Wno-error=...

    CHECK_C_FLAG(Wno-error=c11-extensions VARS c11_extensions)
    if(c11_extensions)
      # If we're going strict, suppress C11 warnings about isnan due
      # to clang issue: https://llvm.org/bugs/show_bug.cgi?id=17788
      CHECK_C_FLAG(Wno-c11-extensions)
      CHECK_CXX_FLAG(Wno-c11-extensions)
    endif(c11_extensions)

    # Add the flag that actually turns warnings into errors
    CHECK_C_FLAG(Werror)
    CHECK_CXX_FLAG(Werror)
  endif(BRLCAD_ENABLE_STRICT)

  # End detection of flags intended for BRL-CAD use.  Make sure all variables have
  # their appropriate values written to the cache - otherwise, DiffCache will see
  # differences and update the COUNT file.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C Compiler flags used by all targets" FORCE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C++ Compiler flags used by all targets" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}" CACHE STRING "Linker flags used by all shared library targets" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Linker flags used by all exe targets" FORCE)
  mark_as_advanced(CMAKE_C_FLAGS)
  mark_as_advanced(CMAKE_CXX_FLAGS)
  mark_as_advanced(CMAKE_SHARED_LINKER_FLAGS)
  mark_as_advanced(CMAKE_EXE_LINKER_FLAGS)
  if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
    set(CMAKE_C_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_C_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "C Compiler flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
    set(CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "C++ Compiler flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "Linker flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER}}" CACHE STRING "Exe linker flags used for ${CMAKE_BUILD_TYPE} builds" FORCE)
    mark_as_advanced(CMAKE_C_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER})
    mark_as_advanced(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER})
  endif(CMAKE_BUILD_TYPE)

  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set(CMAKE_C_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_C_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "C Compiler flags used for ${CFG_TYPE} builds" FORCE)
    set(CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "C++ Compiler flags used for ${CFG_TYPE} builds" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "Linker flags used for ${CFG_TYPE} builds" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER} "${CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER}}" CACHE STRING "Exe linker flags used for ${CFG_TYPE} builds" FORCE)
    mark_as_advanced(CMAKE_C_FLAGS_${CFG_TYPE_UPPER})
    mark_as_advanced(CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER})
    mark_as_advanced(CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER})
    mark_as_advanced(CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})

  # TODO - figure out if this should be integrated above
  CHECK_COMPILER_FLAG(C "-Wunused-const-variable" HAVE_C_WUNUSED_CONST_VARIABLE)

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
set(CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT} CACHE STRING "Threads")
set(CMAKE_USE_WIN32_THREADS_INIT ${CMAKE_USE_WIN32_THREADS_INIT} CACHE STRING "Threads")
set(CMAKE_USE_PTHREADS_INIT ${CMAKE_USE_PTHREADS_INIT} CACHE STRING "Threads")
set(CMAKE_HP_PTHREADS_INIT ${CMAKE_HP_PTHREADS_INIT} CACHE STRING "Threads")

# Check for the C++ STL library - need to link it explicitly in
# some compilation situations
find_package(STL)

# Check for an MPI library
find_package(MPI)

# Check for the daemon function in -lbsd and/or -lc for adrt
BRLCAD_CHECK_LIBRARY(BSD bsd daemon)
BRLCAD_CHECK_LIBRARY(BSD c daemon)

# Check for CoreFoundation and Cocoa on Apple
if(APPLE)
  include(CMakeFindFrameworks)
  CMAKE_FIND_FRAMEWORKS(Foundation)
  if(Foundation_FRAMEWORKS)
    set(Foundation_LIBRARIES "-framework Foundation" CACHE FILEPATH "Foundation framework" FORCE)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_FOUNDATION_FOUNDATION_H 1\n")
  endif(Foundation_FRAMEWORKS)
  mark_as_advanced(CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
  mark_as_advanced(Foundation_LIBRARIES)
endif(APPLE)

# Mark X11 as available if it is enabled and we find Xlib.h
if(BRLCAD_ENABLE_X11)
  message("X11 detected and enabled")
  if(X11_Xlib_INCLUDE_PATH)
    set(HAVE_X11_XLIB_H 1)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_X11_XLIB_H 1\n")
    message("Defining HAVE_X11_XLIB_H")
  endif(X11_Xlib_INCLUDE_PATH)
  if(X11_Xi_INCLUDE_PATH)
    set(HAVE_X11_EXTENSIONS_XINPUT_H 1)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_X11_EXTENSIONS_XINPUT_H 1\n")
    message("Defining HAVE_X11_EXTENSIONS_XINPUT_H")
  endif(X11_Xi_INCLUDE_PATH)
endif(BRLCAD_ENABLE_X11)

# math library
BRLCAD_CHECK_LIBRARY(M m cos)

# uuid library
find_package(UUID)
if (UUID_FOUND)
  BRLCAD_CHECK_LIBRARY(UUID uuid uuid_generate)
endif (UUID_FOUND)

# network socket library (linux, bsd)
BRLCAD_CHECK_LIBRARY(SOCKET socket socket)

# network socket library (solaris)
BRLCAD_CHECK_LIBRARY(NSL nsl gethostbyaddr)

# network socket library (haiku, beos)
BRLCAD_CHECK_LIBRARY(NETWORK network socket)

# malloc library
BRLCAD_CHECK_LIBRARY(MALLOC c mallopt)
BRLCAD_CHECK_LIBRARY(MALLOC malloc mallopt)

# dynamic link library
BRLCAD_CHECK_LIBRARY(DL dl dlopen)

# Solaris lexer library
BRLCAD_CHECK_LIBRARY(SOLARIS_LEXER l yyless)

# timeSetEvent in Windows memory management
if("${HAVE_TIMESETEVENT}" MATCHES "^${HAVE_TIMESETEVENT}$")
  # TODO - replace this cache approach with the CMakePushCheckState module
  set(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "winmm.lib")
  check_c_source_compiles("#include <windows.h>\nint main() {(void)timeSetEvent(1000, 100, (LPTIMECALLBACK)NULL, (DWORD_PTR)NULL, TIME_ONESHOT);}" HAVE_TIMESETEVENT)
  if(HAVE_TIMESETEVENT)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TIMESETEVENT 1\n")
    set(WINMM_LIBRARY "winmm.lib")
  endif(HAVE_TIMESETEVENT)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
endif("${HAVE_TIMESETEVENT}" MATCHES "^${HAVE_TIMESETEVENT}$")

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
CONFIG_H_APPEND(BRLCAD "#define STDC_HEADERS 1\n")

# AC_HEADER_SYS_WAIT
BRLCAD_HEADER_SYS_WAIT()

# dirent.h is POSIX.1, but not present on Windows (grr)
# so we need to check for it
BRLCAD_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)

BRLCAD_INCLUDE_FILE(arpa/inet.h HAVE_ARPA_INET_H)
BRLCAD_INCLUDE_FILE(direct.h HAVE_DIRECT_H)
BRLCAD_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
BRLCAD_INCLUDE_FILE(dslib.h HAVE_DSLIB_H)
BRLCAD_INCLUDE_FILE(emmintrin.h HAVE_EMMINTRIN_H)
BRLCAD_INCLUDE_FILE(getopt.h HAVE_GETOPT_H)
BRLCAD_INCLUDE_FILE(gl/device.h HAVE_GL_DEVICE_H)
BRLCAD_INCLUDE_FILE(gl/glext.h HAVE_GL_GLEXT_H)
BRLCAD_INCLUDE_FILE(gl/wglext.h HAVE_GL_WGLEXT_H)
BRLCAD_INCLUDE_FILE(grp.h HAVE_GRP_H)
BRLCAD_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
BRLCAD_INCLUDE_FILE(io.h HAVE_IO_H)
BRLCAD_INCLUDE_FILE(libgen.h HAVE_LIBGEN_H)
# BRLCAD_INCLUDE_FILE(libproc.h HAVE_LIBPROC_H) - see below
BRLCAD_INCLUDE_FILE(mach/thread_policy.h HAVE_MACH_THREAD_POLICY_H)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
BRLCAD_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
BRLCAD_INCLUDE_FILE(poll.h HAVE_POLL_H)
BRLCAD_INCLUDE_FILE(process.h HAVE_PROCESS_H)
BRLCAD_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
BRLCAD_INCLUDE_FILE(pthread_np.h HAVE_PTHREAD_NP_H)
BRLCAD_INCLUDE_FILE(pwd.h HAVE_PWD_H)
BRLCAD_INCLUDE_FILE(rle.h HAVE_RLE_H)
BRLCAD_INCLUDE_FILE(sched.h HAVE_SCHED_H)
BRLCAD_INCLUDE_FILE(sgtty.h HAVE_SGTTY_H)
BRLCAD_INCLUDE_FILE(signal.h HAVE_SIGNAL_H)
BRLCAD_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
BRLCAD_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
BRLCAD_INCLUDE_FILE(string.h HAVE_STRING_H)
BRLCAD_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
BRLCAD_INCLUDE_FILE(strsafe.h HAVE_STRSAFE_H)
BRLCAD_INCLUDE_FILE(sys/_ioctl.h HAVE_SYS__IOCTL_H)
BRLCAD_INCLUDE_FILE(sys/cpuset.h HAVE_SYS_CPUSET_H)
BRLCAD_INCLUDE_FILE(sys/file.h HAVE_SYS_FILE_H)
BRLCAD_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H)
BRLCAD_INCLUDE_FILE(sys/ioctl_compat.h HAVE_SYS_IOCTL_COMPAT_H)
BRLCAD_INCLUDE_FILE(sys/ipc.h HAVE_SYS_IPC_H)
BRLCAD_INCLUDE_FILE(sys/machd.h HAVE_SYS_MACHD_H)
BRLCAD_INCLUDE_FILE(sys/mman.h HAVE_SYS_MMAN_H)
BRLCAD_INCLUDE_FILE(sys/mount.h HAVE_SYS_MOUNT_H)
BRLCAD_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)
BRLCAD_INCLUDE_FILE(sys/prctl.h HAVE_SYS_PRCTL_H)
BRLCAD_INCLUDE_FILE(sys/resource.h HAVE_SYS_RESOURCE_H)
BRLCAD_INCLUDE_FILE(sys/sched.h HAVE_SYS_SCHED_H)
BRLCAD_INCLUDE_FILE(sys/select.h HAVE_SYS_SELECT_H)
BRLCAD_INCLUDE_FILE(sys/shm.h HAVE_SYS_SHM_H)
BRLCAD_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
BRLCAD_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
BRLCAD_INCLUDE_FILE(sys/sysinfo.h HAVE_SYS_SYSINFO_H)
BRLCAD_INCLUDE_FILE(sys/sysmp.h HAVE_SYS_SYSMP_H)
BRLCAD_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
BRLCAD_INCLUDE_FILE(sys/times.h HAVE_SYS_TIMES_H)
BRLCAD_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
BRLCAD_INCLUDE_FILE(sys/uio.h HAVE_SYS_UIO_H)
BRLCAD_INCLUDE_FILE(sys/un.h HAVE_SYS_UN_H)
BRLCAD_INCLUDE_FILE(sys/utsname.h HAVE_SYS_UTSNAME_H)
BRLCAD_INCLUDE_FILE(sys/wait.h HAVE_SYS_WAIT_H)
BRLCAD_INCLUDE_FILE(syslog.h HAVE_SYSLOG_H)
BRLCAD_INCLUDE_FILE(ulocks.h HAVE_ULOCKS_H)
BRLCAD_INCLUDE_FILE(uuid.h HAVE_UUID_H) # for uuid_generate() on BSD
BRLCAD_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
BRLCAD_INCLUDE_FILE(uuid/uuid.h HAVE_UUID_UUID_H) # for uuid_generate() on Mac & Linux
BRLCAD_INCLUDE_FILE(windows.h HAVE_WINDOWS_H) # for QueryPerformanceCounter() on Windows

# custom sys/sysctl.h test due to BSDisms when compiling
check_c_source_compiles("typedef void *rusage_info_t;\ntypedef unsigned char u_char;\ntypedef unsigned int u_int;\ntypedef unsigned long u_long;\ntypedef unsigned short u_short;\n#define SOCK_MAXADDRLEN 255\n#include <sys/types.h>\n#include <sys/sysctl.h>\nint main() { return 0; }" HAVE_SYS_SYSCTL_H)
if(HAVE_SYS_SYSCTL_H)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_SYS_SYSCTL_H 1\n")
endif(HAVE_SYS_SYSCTL_H)

# custom libproc.h test due to BSDisms when compiling
check_c_source_compiles("typedef void *rusage_info_t;\ntypedef unsigned char u_char;\ntypedef unsigned int u_int;\ntypedef unsigned long u_long;\ntypedef unsigned short u_short;\n#define SOCK_MAXADDRLEN 255\n#include <sys/types.h>\n#include <libproc.h>\nint main() { return 0; }" HAVE_LIBPROC_H)
if(HAVE_LIBPROC_H)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_LIBPROC_H 1\n")
endif(HAVE_LIBPROC_H)

BRLCAD_INCLUDE_FILE(termios.h HAVE_TERMIOS_H)
BRLCAD_INCLUDE_FILE(termio.h HAVE_TERMIO_H)

# C++
BRLCAD_INCLUDE_FILE_CXX(istream HAVE_ISTREAM)
BRLCAD_INCLUDE_FILE_CXX(limits HAVE_LIMITS)

# Other special-case tests that need custom macros
BRLCAD_CHECK_BASENAME()
BRLCAD_CHECK_DIRNAME()

# OpenGL headers
if(BRLCAD_ENABLE_OPENGL)
  if(OPENGL_INCLUDE_DIR_GL)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_GL_GL_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GL)
  if(OPENGL_INCLUDE_DIR_GLX)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_GL_GLX_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GLX)
endif(BRLCAD_ENABLE_OPENGL)

# may have the header, but ensure it works in pedantic mode (gcc bug)
if(HAVE_EMMINTRIN_H)
  check_c_source_compiles("#include <emmintrin.h>\nint main() { return 0; }" HAVE_EMMINTRIN)
  if(HAVE_EMMINTRIN)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_EMMINTRIN 1\n")
  endif(HAVE_EMMINTRIN)
endif(HAVE_EMMINTRIN_H)

# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*          Stage 6 of 9 - Check for Types/Structures      *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

BRLCAD_STRUCT_MEMBER("struct stat" st_blksize sys/stat.h STRUCT_STAT_ST_BLKSIZE)
BRLCAD_STRUCT_MEMBER("struct stat" st_blocks sys/stat.h STRUCT_STAT_ST_BLOCKS)
BRLCAD_STRUCT_MEMBER("struct stat" st_rdev sys/stat.h STRUCT_STAT_ST_RDEV)

# timespec can come in through sys/select.h
if(HAVE_SYS_SELECT_H)
  BRLCAD_STRUCT_MEMBER("struct timespec" tv_sec sys/select.h STRUCT_TIMESPEC)
endif(HAVE_SYS_SELECT_H)

# socklen_t
BRLCAD_TYPE_SIZE("socklen_t" "")
if(NOT HAVE_SOCKLEN_T)
  BRLCAD_TYPE_SIZE("socklen_t" "sys/types.h")
  if(NOT HAVE_SOCKLEN_T)
    BRLCAD_TYPE_SIZE("socklen_t" "sys/socket.h")
  endif(NOT HAVE_SOCKLEN_T)
endif(NOT HAVE_SOCKLEN_T)
BRLCAD_TYPE_SIZE("cpu_set_t" "sched.h")

BRLCAD_TYPE_SIZE("int" "")
BRLCAD_TYPE_SIZE("long" "")
BRLCAD_TYPE_SIZE("long long" "")
BRLCAD_TYPE_SIZE("off_t" "")
BRLCAD_TYPE_SIZE("ptrdiff_t" "")
BRLCAD_TYPE_SIZE("size_t" "")
BRLCAD_TYPE_SIZE("ssize_t" "")
BRLCAD_TYPE_SIZE("uint64_t" "")
BRLCAD_TYPE_SIZE("uintptr_t" "")
BRLCAD_TYPE_SIZE("sig_t" "signal.h")

# see if compile settings disable historic unsigned bsd types.
# typically disabled on posix conformant systems.
BRLCAD_TYPE_SIZE("u_char" "sys/types.h")
BRLCAD_TYPE_SIZE("u_int" "sys/types.h")
BRLCAD_TYPE_SIZE("u_long" "sys/types.h")
BRLCAD_TYPE_SIZE("u_short" "sys/types.h")
if (HAVE_U_CHAR AND HAVE_U_INT AND HAVE_U_LONG AND HAVE_U_SHORT)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_U_TYPES 1\n")
endif (HAVE_U_CHAR AND HAVE_U_INT AND HAVE_U_LONG AND HAVE_U_SHORT)

# If we have an off_t that's too small, we'll need to deal with that in order to
# support large files successfully.
if (CMAKE_SIZEOF_VOID_P AND HAVE_OFF_T)
  if (${CMAKE_SIZEOF_VOID_P} GREATER ${HAVE_OFF_T})
    if (NOT DEFINED OFF_T_SIZE_MISMATCH)
      message("Note: platform void pointer size (${CMAKE_SIZEOF_VOID_P}) is greater than off_t size (${HAVE_OFF_T})")
    endif (NOT DEFINED OFF_T_SIZE_MISMATCH)
    CONFIG_H_APPEND(BRLCAD "#define OFF_T_SIZE_MISMATCH 1\n")
    set(OFF_T_SIZE_MISMATCH  1 CACHE INTERNAL "Have off_t that is too small for platform.")
  endif (${CMAKE_SIZEOF_VOID_P} GREATER ${HAVE_OFF_T})
  if (CMAKE_WORD_SIZE STREQUAL "64BIT")
    CONFIG_H_APPEND(BRLCAD "#define HAVE_OFF_T_64BIT 1\n")
  endif (CMAKE_WORD_SIZE STREQUAL "64BIT")
endif (CMAKE_SIZEOF_VOID_P AND HAVE_OFF_T)

# see if we have a TLS intrinsic, first check C++11 compliance
check_cxx_source_compiles("static thread_local int i = 0; int main() { return i; }" HAVE_THREAD_LOCAL)
if (HAVE_THREAD_LOCAL)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_THREAD_LOCAL 1\n")
else (HAVE_THREAD_LOCAL)
  # try GCC except Mac OS X
  include(CheckCXXSourceRuns)
  check_cxx_source_runs("static __thread int i = 0; int main() { return i; }" HAVE___THREAD)
  if (HAVE___THREAD)
    CONFIG_H_APPEND(BRLCAD "#define HAVE___THREAD 1\n")
  else (HAVE___THREAD)
    # try Windows
    check_cxx_source_compiles("static __declspec(thread) int i = 0; int main() { return i; }" HAVE___DECLSPEC_THREAD)
    if (HAVE___DECLSPEC_THREAD)
      CONFIG_H_APPEND(BRLCAD "#define HAVE___DECLSPEC_THREAD 1\n")
    endif (HAVE___DECLSPEC_THREAD)
  endif (HAVE___THREAD)
endif (HAVE_THREAD_LOCAL)

# Test for C++11 features that we need - CMake will sometimes assert that a
# C++11 compiler flag exists, but the actual compiler support for C++11 language
# ends up lacking in practice.
include(CheckCXX11Features)
cxx11_check_feature("nullptr" HAS_CXX11_NULLPTR)
cxx11_check_feature("lib_regex" HAS_CXX11_LIB_REGEX)

# see if the compiler supports %z as a size_t print width specifier
BRLCAD_CHECK_PERCENT_Z()

# see if the compiler supports [static] array hints
BRLCAD_CHECK_STATIC_ARRAYS()

# see if we have a TLS intrinsic, first check C++11 compliance
check_cxx_source_compiles("static thread_local int i = 0; int main() { return i; }" HAVE_THREAD_LOCAL)


# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*              Stage 7 of 9 - Check for Functions         *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

include(CheckSymbolExists)

BRLCAD_FUNCTION_EXISTS(XQueryExtension)
BRLCAD_FUNCTION_EXISTS(_putenv_s)
BRLCAD_FUNCTION_EXISTS(_splitpath)
BRLCAD_FUNCTION_EXISTS(_strtoi64)
BRLCAD_FUNCTION_EXISTS(alarm)
BRLCAD_FUNCTION_EXISTS(asinh REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(confstr) # darwin/mac
BRLCAD_FUNCTION_EXISTS(dlopen)
BRLCAD_FUNCTION_EXISTS(dladdr)
BRLCAD_FUNCTION_EXISTS(drand48)
BRLCAD_FUNCTION_EXISTS(fchmod)
BRLCAD_FUNCTION_EXISTS(fdopen)
BRLCAD_FUNCTION_EXISTS(fmemopen)
BRLCAD_FUNCTION_EXISTS(fseeko) # implies ftello
BRLCAD_FUNCTION_EXISTS(fsync)
BRLCAD_FUNCTION_EXISTS(funopen)
BRLCAD_FUNCTION_EXISTS(getcwd)
BRLCAD_FUNCTION_EXISTS(getegid)
BRLCAD_FUNCTION_EXISTS(getenv_s)
BRLCAD_FUNCTION_EXISTS(geteuid)
BRLCAD_FUNCTION_EXISTS(gethostbyname)
BRLCAD_FUNCTION_EXISTS(gethostname)
BRLCAD_FUNCTION_EXISTS(getloadavg)
BRLCAD_FUNCTION_EXISTS(getopt_long)
BRLCAD_FUNCTION_EXISTS(getpid)
BRLCAD_FUNCTION_EXISTS(getprogname)
BRLCAD_FUNCTION_EXISTS(gettimeofday)
BRLCAD_FUNCTION_EXISTS(htonl)
BRLCAD_FUNCTION_EXISTS(htonll)
BRLCAD_FUNCTION_EXISTS(hypot REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(isascii)
BRLCAD_FUNCTION_EXISTS(isinf REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(isnan REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(logb REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(lrand48)
BRLCAD_FUNCTION_EXISTS(lseek)
BRLCAD_FUNCTION_EXISTS(mkstemp)
BRLCAD_FUNCTION_EXISTS(modff REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(nextafter REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(nextafterf REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(ntohll)
BRLCAD_FUNCTION_EXISTS(pipe)
BRLCAD_FUNCTION_EXISTS(popen) # implies pclose
BRLCAD_FUNCTION_EXISTS(posix_memalign) # IEEE Std 1003.1-2001
BRLCAD_FUNCTION_EXISTS(proc_pidpath) # Mac OS X
BRLCAD_FUNCTION_EXISTS(program_invocation_name)
BRLCAD_FUNCTION_EXISTS(random)
BRLCAD_FUNCTION_EXISTS(realpath)
BRLCAD_FUNCTION_EXISTS(rint REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(setenv)
BRLCAD_FUNCTION_EXISTS(setpgid)
BRLCAD_FUNCTION_EXISTS(setpriority)
BRLCAD_FUNCTION_EXISTS(setsid)
BRLCAD_FUNCTION_EXISTS(shmat)
BRLCAD_FUNCTION_EXISTS(shmctl)
BRLCAD_FUNCTION_EXISTS(shmdt)
BRLCAD_FUNCTION_EXISTS(shmget)
BRLCAD_FUNCTION_EXISTS(snprintf)
BRLCAD_FUNCTION_EXISTS(srand48)
BRLCAD_FUNCTION_EXISTS(strcasecmp)
BRLCAD_FUNCTION_EXISTS(strdup)
BRLCAD_FUNCTION_EXISTS(strlcat)
BRLCAD_FUNCTION_EXISTS(strlcpy)
BRLCAD_FUNCTION_EXISTS(strncasecmp)
BRLCAD_FUNCTION_EXISTS(strtoll)
BRLCAD_FUNCTION_EXISTS(sync)
BRLCAD_FUNCTION_EXISTS(sysconf)
BRLCAD_FUNCTION_EXISTS(sysctl)
BRLCAD_FUNCTION_EXISTS(sysmp)
BRLCAD_FUNCTION_EXISTS(time)
BRLCAD_FUNCTION_EXISTS(toascii)
BRLCAD_FUNCTION_EXISTS(tzset)
BRLCAD_FUNCTION_EXISTS(uname)
BRLCAD_FUNCTION_EXISTS(vfork)
BRLCAD_FUNCTION_EXISTS(vsnprintf)
BRLCAD_FUNCTION_EXISTS(vsscanf)
BRLCAD_FUNCTION_EXISTS(wait)
BRLCAD_FUNCTION_EXISTS(writev)

# ALLOCA test - based on AC_FUNC_ALLOCA
BRLCAD_ALLOCA()

# We may want access to some POSIX functions even when compiling in strict
# mode, if they are present. For those cases, we need to record the DECL
# test result.
BRLCAD_FUNCTION_EXISTS(kill)
BRLCAD_FUNCTION_EXISTS(fileno)

# test for daemon(), which is deprecated on Mac OS X 10.5+
BRLCAD_FUNCTION_EXISTS(daemon)
if (HAVE_DAEMON)
  check_c_source_compiles("#include <stdlib.h>\nint main() { (void)daemon; return 0; }" HAVE_WORKING_DAEMON_FUNCTION)
  if (HAVE_WORKING_DAEMON_FUNCTION)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_WORKING_DAEMON_FUNCTION 1\n")
  endif (HAVE_WORKING_DAEMON_FUNCTION)
endif (HAVE_DAEMON)

# ntohll and htonll may be harder to spot - do some extra tests
if("${HAVE_NTOHLL}" STREQUAL "")
  CHECK_SYMBOL_EXISTS(ntohll "sys/_endian.h" HAVE_NTOHLL_SYS__ENDIAN)
  if(HAVE_NTOHLL_SYS__ENDIAN)
    set(HAVE_NTOHLL 1 CACHE INTERNAL "Have function NTOHLL")
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_NTOHLL 1\n")
  endif(HAVE_NTOHLL_SYS__ENDIAN)
endif("${HAVE_NTOHLL}" STREQUAL "")
if("${HAVE_HTONLL}" STREQUAL "")
  CHECK_SYMBOL_EXISTS(htonll "sys/_endian.h" HAVE_HTONLL_SYS__ENDIAN)
  if(HAVE_HTONLL_SYS__ENDIAN)
    set(HAVE_HTONLL 1 CACHE INTERNAL "Have function HTONLL")
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_HTONLL 1\n")
  endif(HAVE_HTONLL_SYS__ENDIAN)
endif("${HAVE_HTONLL}" STREQUAL "")

# Test for _snprintf() if we don't have snprintf
if(NOT HAVE_SNPRINTF)
  BRLCAD_FUNCTION_EXISTS(_snprintf)
  if(HAVE__SNPRINTF)
    CONFIG_H_APPEND(BRLCAD "#define snprintf _snprintf\n")
  endif(HAVE__SNPRINTF)
endif(NOT HAVE_SNPRINTF)

if(HAVE_REALPATH)
 #check_c_source_runs("#include<limits.h>\n#include <stdlib.h>\nint main() { char dir[PATH_MAX]; const char *d = \"/tmp/REALPATH_TEST_PATH\"; d = (const char *)realpath(d, dir); return 0; }" HAVE_WORKING_REALPATH)
 #if(HAVE_WORKING_REALPATH)
 #  CONFIG_H_APPEND(BRLCAD "#define HAVE_WORKING_REALPATH 1\n")
 #endif(HAVE_WORKING_REALPATH)
endif(HAVE_REALPATH)

# GetFullPathName
check_c_source_compiles("
#include <windows.h>
int main() {
const char *path = \"Windows\";
char *resolved_path;
(void)GetFullPathName(path, MAX_PATH, resolved_path, NULL);
return 0;
}
" HAVE_GETFULLPATHNAME)
if(HAVE_GETFULLPATHNAME)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_GETFULLPATHNAME 1\n")
endif(HAVE_GETFULLPATHNAME)

# GetCurrentProcessId
check_c_source_compiles("
#include <windows.h>
int main() {
DWORD cpid = GetCurrentProcessId();
return 0;
}
" HAVE_GETCURRENTPROCESSID)
if(HAVE_GETCURRENTPROCESSID)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_GETCURRENTPROCESSID 1\n")
endif(HAVE_GETCURRENTPROCESSID)

# functionality tests for lrint
set(lrint_test "long int lrint(double); int main() {return lrint(3.14);}")
set(lrint_test_negative "long int lrint(double); int main() {return lrint(-1);}")
# decl test for lrint
set(lrint_decl_test "#include <math.h>\nint main() { (void)lrint(0); return 0; }")
BRLCAD_FUNCTION_EXISTS(lrint DECL_TEST_SRCS lrint_decl_test
  WORKING_TEST_SRCS lrint_test lrint_test_negative
  REQUIRED_LIBS ${M_LIBRARY})
if(HAVE_DECL_LRINT)
  CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_DECL_LRINT 1\n")
endif(HAVE_DECL_LRINT)
if(NOT HAVE_LRINT AND NOT DEFINED HAVE_WORKING_LRINT_MACRO)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES ${M_LIBRARY})
  # NOTE: this must match the lrint #define in include/common.h
  check_c_source_compiles("#define lrint(_x) (((_x) < 0.0) ? ceil((_x)-0.5) : floor((_x)+0.5))\nint main() {return lrint(3.14);}" HAVE_WORKING_LRINT_MACRO)
endif(NOT HAVE_LRINT AND NOT DEFINED HAVE_WORKING_LRINT_MACRO)
if(HAVE_WORKING_LRINT_MACRO)
  CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_WORKING_LRINT_MACRO 1\n")
endif(HAVE_WORKING_LRINT_MACRO)

# test for tgamma
set(tgamma_test "#include <math.h>\nint main() {double tga = tgamma(3.14); return 0;}")
BRLCAD_FUNCTION_EXISTS(tgamma COMPILE_TEST_SRCS tgamma_test REQUIRED_LIBS ${M_LIBRARY})

# test for gethostbyaddr
set(gethostbyaddr_test "#include<stdlib.h>\n#include <netdb.h>\nint main() {(void)gethostbyaddr(NULL, 0, AF_INET); return 0;}")
BRLCAD_FUNCTION_EXISTS(gethostbyaddr DECL_TEST_SRCS gethostbyaddr_test REQUIRED_LIBS ${NL_LIBRARY})

# Check for logb in C++
check_cxx_source_compiles("#include <cmath>\nint main() {return logb(3.14);}" HAVE_CXX_LOGB)
CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_CXX_LOGB 1\n")

#-----------------------------------------------------------------------

# On Windows, we need to check for hypot etc.  This test pertains
# to the windows specific config file, not CONFIG_H_FILE - hence,
# just run the test and it will be handled by configure_file later.
if(WIN32)

  # consider all warnings as errors (MSVC)
  # CHECK_C_FLAG(WX)
  # CHECK_CXX_FLAG(WX)

  CHECK_SYMBOL_EXISTS(hypot "math.h" HAVE_HYPOT)
  if(NOT HAVE_HYPOT)
    set(hypot 1)
  endif(NOT HAVE_HYPOT)

  CHECK_SYMBOL_EXISTS(asinh "math.h" HAVE_ASINH)
  if(NOT HAVE_ASINH)
    set(asinh 1)
  endif(NOT HAVE_ASINH)

  CHECK_SYMBOL_EXISTS(isnan "math.h" HAVE_ISNAN)
  if(HAVE_ISNAN)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ISNAN 1\n")
  else(HAVE_ISNAN)
    CHECK_SYMBOL_EXISTS(_isnan "float.h" HAVE__ISNAN)
    if (HAVE__ISNAN)
      CONFIG_H_APPEND(BRLCAD "#define HAVE__ISNAN 1\n")
    endif (HAVE__ISNAN)
  endif(HAVE_ISNAN)

  CHECK_SYMBOL_EXISTS(isinf "math.h" HAVE_ISINF)
  if(NOT HAVE_ISINF)
    set(isinf 1)
  endif(NOT HAVE_ISINF)

  CHECK_SYMBOL_EXISTS(rint "math.h" HAVE_RINT)

  CHECK_SYMBOL_EXISTS(fmax "math.h" HAVE_FMAX)
  if(NOT HAVE_FMAX)
    set(fmax 1)
  endif(NOT HAVE_FMAX)

  CHECK_SYMBOL_EXISTS(nextafterf "math.h" HAVE_NEXTAFTERF)
  if(NOT HAVE_NEXTAFTERF)
    set(nextafterf 1)
  endif(NOT HAVE_NEXTAFTERF)

  CHECK_SYMBOL_EXISTS(nextafterl "math.h" HAVE_NEXTAFTERL)
  if(NOT HAVE_NEXTAFTERL)
    set(nextafterl 1)
  endif(NOT HAVE_NEXTAFTERL)

  BRLCAD_FUNCTION_EXISTS(_fseeki64)
  BRLCAD_FUNCTION_EXISTS(_lseeki64)
  BRLCAD_FUNCTION_EXISTS(_ftelli64)

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
DISTCLEAN("${BRLCAD_CNT_FILE}")
set(BRLCAD_COMPILE_COUNT ${buildCounter})

# DATE - RFC2822 timestamp
set(BRLCAD_COMPILE_DATE \"${CONFIG_DATESTAMP}\")

# Add definition HAVE_UNISTD_H to try_run for next couple tests, if present
if(HAVE_UNISTD_H)
  set(UNISTD_DEF "-DHAVE_UNISTD_H")
endif(HAVE_UNISTD_H)

# HOST
if(HAVE_GETHOSTNAME)
  set(GETHOSTNAME_DEF "-DHAVE_GETHOSTNAME")
endif(HAVE_GETHOSTNAME)
if(HAVE_DECL_GETHOSTNAME)
  set(GETHOSTNAME_DECL_DEF "-DHAVE_DECL_GETHOSTNAME")
endif(HAVE_DECL_GETHOSTNAME)

set(report_hostname_src "
#include <stdio.h>
#ifdef HAVE_UNISTD_H
  #include <unistd.h>
  #define GETHOSTNAME_ERROR -1
#else
  #include <WinSock2.h>
  #define GETHOSTNAME_ERROR SOCKET_ERROR
#endif

#define MAX_HOSTNAME 2048

/* strict c89 doesn't declare gethostname() */
#if defined(HAVE_GETHOSTNAME) && !defined(HAVE_DECL_GETHOSTNAME) && !defined(__cplusplus) && !defined(_WINSOCKAPI_)
extern int gethostname(char *name, size_t len);
#endif

int main(void) {
#if defined(HAVE_GETHOSTNAME)
{
    int ret; char hostname[MAX_HOSTNAME];
  #if !defined(HAVE_UNISTD_H)
    WORD wVersionRequested; WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1); WSAStartup(wVersionRequested, &wsaData);
  #endif
    ret = gethostname(hostname, MAX_HOSTNAME);
    if (ret != GETHOSTNAME_ERROR) {printf(\"%s\", hostname);} else { printf(\"unknown\"); }
}
#else
    printf(\"unknown\");
#endif
    return 0;
}
")

file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c" "${report_hostname_src}")
try_run(RH_RESULT RH_COMPILED "${CMAKE_BINARY_DIR}/CMakeTmp"
  "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c"
  COMPILE_DEFINITIONS "${UNISTD_DEF}" "${GETHOSTNAME_DEF}" "${GETHOSTNAME_DECL_DEF}"
  LINK_LIBRARIES "${WS2_32_LIBRARY}"
  COMPILE_OUTPUT_VARIABLE RH_COMPILE_MSGS
  RUN_OUTPUT_VARIABLE BRLCAD_COMPILE_HOSTNAME)
if(NOT RH_COMPILED)
  message(FATAL_ERROR "Could not build hostname reporting utility: ${RH_COMPILE_MSGS}")
endif(NOT RH_COMPILED)
if(RH_RESULT)
  message(FATAL_ERROR "Could not run hostname reporting utility: ${BRLCAD_COMPILE_HOSTNAME}")
endif(RH_RESULT)
string(STRIP ${BRLCAD_COMPILE_HOSTNAME} BRLCAD_COMPILE_HOSTNAME)
file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c")

# USER
set(report_username_src "
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
   #include <unistd.h>
#endif
int main(void) {
  char *name = getenv(\"USERNAME\");
  if ((name == 0) || (strlen(name) == 0)) name = getenv(\"USER\");
  if ((name == 0) || (strlen(name) == 0)) name = \"unknown\";
  printf(\"%s\", name);
  return 0;
}
")
file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c" "${report_username_src}")
# Add definition HAVE_UNISTD_H to try_run
try_run(RU_RESULT RU_COMPILED "${CMAKE_BINARY_DIR}/CMakeTmp"
  "${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c"
  COMPILE_DEFINITIONS "${UNISTD_DEF}"
  COMPILE_OUTPUT_VARIABLE RU_COMPILE_MSGS
  RUN_OUTPUT_VARIABLE BRLCAD_COMPILE_USER)
if(NOT RU_COMPILED)
  message(FATAL_ERROR "Could not build username reporting utility: ${RU_COMPILE_MSGS}")
endif(NOT RU_COMPILED)
if(RU_RESULT)
  message(FATAL_ERROR "Could not run username reporting utility: ${BRLCAD_COMPILE_USER}")
endif(RU_RESULT)
string(STRIP ${BRLCAD_COMPILE_USER} BRLCAD_COMPILE_USER)
file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c")

if(MSVC)
  # By default, do not warn when built on machines using only VS Express
  # From: http://www.cmake.org/pipermail/cmake/2011-May/044166.html
  if(NOT DEFINED CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
  endif()
  # For Windows, we need some dlls to be redistributed with the installer.
  include(InstallRequiredSystemLibraries)

  # For portability with older Windows versions - remove this once we are no
  # longer worried about Windows versions older than Windows 7/Windows 2008 R2
  # https://blogs.msdn.microsoft.com/vcblog/2009/08/27/windows-sdk-v7-0v7-0a-incompatibility-workaround/
  add_definitions(-DPSAPI_VERSION=1)

  # Before we finalize, set some specific global linker flags
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /STACK:10000000 /NOLOGO")
endif(MSVC)

#-----------------------------------------------------------------------------
# Before we head into src/other and misc/tools, store all the build
# flags that have been built up.  It's hard to be sure what will and
# won't be set in those directories, so make sure we can restore the
# BRL-CAD flags for the actual BRL-CAD subdirectories.
CACHE_BUILD_FLAGS(_BRLCAD)

# For lower build levels, some of the third party components are not
# needed.  define some variables we can use for testing.
set(BRLCAD_LEVEL2 0)
set(BRLCAD_LEVEL3 0)
if(NOT BRLCAD_ENABLE_TARGETS)
  set(BRLCAD_LEVEL2 1)
  set(BRLCAD_LEVEL3 1)
else(NOT BRLCAD_ENABLE_TARGETS)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 1)
    set(BRLCAD_LEVEL2 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 1)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 2)
    set(BRLCAD_LEVEL3 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 2)
endif(NOT BRLCAD_ENABLE_TARGETS)

# If we're below level 2, we don't need Tcl
if (NOT BRLCAD_LEVEL2)
  set(BRLCAD_ENABLE_TCL OFF CACHE BOOL "Disabled due to level 1 build" FORCE)
endif (NOT BRLCAD_LEVEL2)

# Before we head into src/other and misc/tools, store all the build
# flags that have been built up.  It's hard to be sure what will and
# won't be set in those directories, so make sure we can restore the
# BRL-CAD flags for the actual BRL-CAD subdirectories.
CACHE_BUILD_FLAGS(_BRLCAD)

# At the start, clear the misc/tools subdirs list so repeated
# configures will correctly add the required directories
set(MISC_ADDED_DIRS "" CACHE STRING "initialize 3rd party sub-directories list" FORCE)
mark_as_advanced(MISC_ADDED_DIRS)

# The first order of business is the ext projects
add_subdirectory(src/other/ext)

# Add misc/tools for tools that are used in BRL-CAD's build process
# but are not otherwise usable in BRL-CAD (due to licensing, design
# intent, etc.)  misc/tools must be handled before src/other because
# some of src/other's projects may need tools from misc/tools.
add_subdirectory(misc/tools)

# Now that we've done the system tests with BRL-CAD's compile flags,
# add src/other to pick up any tests it needs.  We must add src/other
# before the main BRL-CAD directories to provide the necessary build
# targets.  Remember that src/other wipes the top level flags generated
# by BRL-CAD for its own subdirectories - it is added after the BRL-CAD
# tests so the CACHE reflects BRL-CAD's test results.
add_subdirectory(src/other)

# Now put back the BRL-CAD flags
RESTORE_CACHED_BUILD_FLAGS(_BRLCAD)

set(TCL_VERSION "8.6")
CONFIG_H_APPEND(BRLCAD "#define TCL_VERSION \"${TCL_VERSION}\"\n")

set(ITCL_VERSION "3.4")
CONFIG_H_APPEND(BRLCAD "#cmakedefine ITCL_VERSION \"${ITCL_VERSION}\"\n")

set(IWIDGETS_VERSION "4.1.1")
CONFIG_H_APPEND(BRLCAD "#cmakedefine IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")

if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*        Stage 9 of 9 - Output and Summarize Config       *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

# Enable CTest Testing support - this is done after src/other is added
# to avoid incorporating src/other CTests into BRL-CAD's own testing.
include(CTest)
mark_as_advanced(BUILD_TESTING)

# We've done the toplevel configure steps, now add the subdirectories
function(verbose_add_subdirectory root dir)
    if(BRLCAD_PRINT_MSGS)
      if(NOT "${root}" STREQUAL "")
	message("-- Adding ${root}/${dir}...")
      else(NOT "${root}" STREQUAL "")
	message("-- Adding ${dir}...")
      endif(NOT "${root}" STREQUAL "")
    endif(BRLCAD_PRINT_MSGS)
    add_subdirectory(${dir})
    if(BRLCAD_PRINT_MSGS)
      if(NOT "${root}" STREQUAL "")
	message("-- Adding ${root}/${dir} - done")
      else(NOT "${root}" STREQUAL "")
	message("-- Adding ${dir} - done")
      endif(NOT "${root}" STREQUAL "")
    endif(BRLCAD_PRINT_MSGS)
endfunction()

verbose_add_subdirectory("" src)
verbose_add_subdirectory("" include)
verbose_add_subdirectory("" sh)
verbose_add_subdirectory("" misc)
if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  verbose_add_subdirectory("" doc)
  verbose_add_subdirectory("" db)
  verbose_add_subdirectory("" bench)
  verbose_add_subdirectory("" regress)
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)

# Restore BRL-CAD configurations, if necessary
if(NOT ENABLE_ALL_CONFIG_TYPES)
  if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
  endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
endif(NOT ENABLE_ALL_CONFIG_TYPES)

# Make sure we have the target dir
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/${DATA_DIR}/data")

# If we're building on Windows with MSVC, use config_win.h file
if(MSVC)
  CONFIG_H_APPEND(BRLCAD "#include \"brlcad/config_win.h\"\n")
endif(MSVC)

# Finalize brlcad_config.h
CONFIG_H_APPEND(BRLCAD "#endif /* __CONFIG_H__ */\n")
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY ${CMAKE_PROJECT_NAME}_CONFIG_H_CONTENTS)
set_source_files_properties(${CONFIG_H_FILE} PROPERTIES GENERATED TRUE)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
BUILD_CFG_HDR(${CONFIG_H_FILE} ${INCLUDE_DIR}/brlcad)

#---------------------------------------------------------------------
# Rules for the toplevel documentation files
set(toplevel_DOCFILES
  AUTHORS
  CHANGES
  COPYING
  HACKING
  INSTALL
  NEWS
  README
  )
BRLCAD_ADDDATA(toplevel_DOCFILES ".")

# Now that everything is configured, print a summary of the build
# settings.
if(NOT BRLCAD_DISABLE_SUMMARY)
  include(BRLCAD_Summary)
  BRLCAD_Summary()
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
add_custom_target(timestamp ALL
    COMMAND "${CMAKE_COMMAND}" -DMSG=\"Build Time:\" -P "${BRLCAD_CMAKE_DIR}/scripts/printtime.cmake"
    DEPENDS ${BUILD_DELTA_START}
    )
set_target_properties(timestamp PROPERTIES FOLDER "Compilation Utilities")
add_custom_target(buildtimedelta ALL
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
  # TODO - once we require 3.15.7 or greater, use list(POP_FRONT) for this...
  list(GET SUBDIRS 0 CDIR)
  list(REMOVE_AT SUBDIRS 0)
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
  if (DIR_TARGETS)
    get_property(DIR_TARGETS DIRECTORY "${ad}" PROPERTY BUILDSYSTEM_TARGETS)
    set(ALL_TARGETS ${ALL_TARGETS} ${DIR_TARGETS})
  endif (DIR_TARGETS)
endforeach(ad ${ALL_DIRS})

# Now, set up the target dependencies for tiemstamp and buildtimedelta.  These
# dependencies will produce a build-tool-independent ordering that gives us the
# timing behavior we want.
foreach(ctarget ${ALL_TARGETS})
  if(NOT ${ctarget} MATCHES "timestamp")
    add_dependencies(${ctarget} timestamp)
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
# This eliminates the need to override add_subdirectory.  Needs CMake 3.7 for
# BINARY_DIR property.
foreach(ad ${ALL_DIRS})
  get_property(BDIR DIRECTORY "${ad}" PROPERTY BINARY_DIR)
  DISTCLEAN("${BDIR}/CMakeFiles")
  DISTCLEAN("${BDIR}/cmake_install.cmake")
  foreach(clearpattern ${DISTCLEAN_OUTFILES})
    DISTCLEAN("${BDIR}/${clearpattern}")
  endforeach(clearpattern ${DISTCLEAN_OUTFILES})
endforeach(ad ${ALL_DIRS})

#------------------------------------------------------------------------------
# To set correct install paths for CMake at build time, rather than CMake
# time, some rather special logic is necessary - a build target that needs
# to be run when the current build type changes, and introspective scripting
# that updates the CMake files themselves to have the correct path.
# Unfortunately, there does not appear to be a way to control this well enough
# from the CMake level - a build-type aware path is technically possible, but
# it does not conform to BRL-CAD's conventions for installation directory
# naming.
if(CMAKE_CONFIGURATION_TYPES AND NOT BRLCAD_IS_SUBBUILD)
  # if we have stale cmake_install.cmake.orig files around, clear them now.
  file(GLOB_RECURSE ALL_CMAKE_INSTALL_FILES_ORIG "*cmake_install.cmake.orig")
  if(ALL_CMAKE_INSTALL_FILES_ORIG)
    file(REMOVE ${ALL_CMAKE_INSTALL_FILES_ORIG})
  endif(ALL_CMAKE_INSTALL_FILES_ORIG)
  # need a build target for this one - install directory may be dependent on configuration, but that won't
  # do for BRL-CAD reporting purposes - must get the fully qualified path at build time.
  configure_file("${BRLCAD_CMAKE_DIR}/multiconfig_path_clean.cmake.in" "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_clean.cmake" @ONLY)
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_clean.cmake")
  configure_file("${BRLCAD_CMAKE_DIR}/multiconfig_path_read.cmake.in" "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_read.cmake" @ONLY)
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_read.cmake")
  if(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH")
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH")
  endif(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH")
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH/${CMAKE_CFG_INTDIR}.done"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_clean.cmake"
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH/${CMAKE_CFG_INTDIR}"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_read.cmake"
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH/${CMAKE_CFG_INTDIR}.done"
    )
  # To make sure multiconfig_path runs in time for the source code builds it might impact, we will need to add the output file as a dependency
  # to the first target to run - the timestamp printing.
  add_custom_target(multiconfig_path DEPENDS "${CMAKE_BINARY_DIR}/CMakeTmp/CURRENT_PATH/${CMAKE_CFG_INTDIR}.done")
  set_target_properties(multiconfig_path PROPERTIES FOLDER "Compilation Utilities")
  add_dependencies(timestamp multiconfig_path)
endif(CMAKE_CONFIGURATION_TYPES AND NOT BRLCAD_IS_SUBBUILD)


# CPack is used to produce tgz files, RPMS, etc.  If SUBBUILD is enabled this
# becomes the responsibility of the parent project.
if(NOT BRLCAD_IS_SUBBUILD)
  include(BRLCAD_CPack)
endif(NOT BRLCAD_IS_SUBBUILD)


if(NOT BRLCAD_IS_SUBBUILD)
  # Some files to ignore for distcheck.  For this case
  # only, we add CMakeLists.txt (others are handled
  # by add_subdirectory wrapper
  set(toplevel_ignore_files
    BUGS
    ChangeLog
    TODO
    configure
    CMakeLists.txt
    )
  CMAKEFILES(${toplevel_ignore_files})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore)
    CMAKEFILES(.gitignore)
  endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore)

  # List any files which may exist for CI testing for distcheck.  TODO -
  # this should be a fixed list, but until we go live with the Git repo
  # use file(GLOB_RECURSE) instead
  file(GLOB_RECURSE GITHUB_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ".github/*")
  CMAKEFILES(${GITHUB_FILES})

  # Handle some toplevel distclean listings
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeCache.txt")
  DISTCLEAN("${CMAKE_BINARY_DIR}/cmakefiles.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/brlcadexec.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/cmake_install.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/install_manifest.txt")
  DISTCLEAN("${CMAKE_BINARY_DIR}/OPTIONS")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeFiles")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp")
  DISTCLEAN("${CMAKE_BINARY_DIR}/configure.new")
  DISTCLEAN("${CMAKE_BINARY_DIR}/INSTALL.new")
  DISTCLEAN("${CMAKE_BINARY_DIR}/include/brlcad_config.h.in")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeDoxyfile.in")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeDoxygenDefaults.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CPackProperties.cmake")
  foreach(clearpattern ${DISTCLEAN_OUTFILES})
    DISTCLEAN("${CMAKE_BINARY_DIR}/${clearpattern}")
  endforeach(clearpattern ${DISTCLEAN_OUTFILES})
  if("${CMAKE_GENERATOR}" MATCHES "Ninja*")
    DISTCLEAN("${CMAKE_BINARY_DIR}/.ninja_log")
    DISTCLEAN("${CMAKE_BINARY_DIR}/.ninja_deps")
  endif("${CMAKE_GENERATOR}" MATCHES "Ninja*")

  # ----------------------------------------------------------------------------
  # Define a distcheck target.  This performs a variety of tests to determine
  # whether BRL-CAD is in a distribution ready state.  Default to the standard
  # set of tests - Debug and Release build configurations
  include(Distcheck)

  # Define some custom distcheck targets for distcheck-full (not run by default)
  #
  # CREATE_DISTCHECK(TARGET_SUFFIX CMAKE_OPTS source_dir build_dir install_dir [custom_template])
  CREATE_DISTCHECK(default_build_type "-DBRLCAD_BUNDLED_LIBS=AUTO" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(no_tcl "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED -DBRLCAD_ENABLE_TCL=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install" distcheck_no_tcl.cmake.in)
  CREATE_DISTCHECK(no_tk "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED -DBRLCAD_ENABLE_TK=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(no_strict "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED -DBRLCAD_ENABLE_STRICT=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(no_object "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED -DUSE_OBJECT_LIBS=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(autodetect_debug "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=AUTO" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(autodetect_release "-DCMAKE_BUILD_TYPE=Release -DBRLCAD_BUNDLED_LIBS=AUTO" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  #CREATE_DISTCHECK(odd_pathnames "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED" "1 Odd_ source dir ++" "1 Odd_ build dir ++" "1 Odd_ install dir ++")
  CREATE_DISTCHECK(in_src_dir "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "install" distcheck_in_src_dir.cmake.in)

  # Now that we're set up and have added the extra targets we want for distcheck-full, define the build targets
  DEFINE_DISTCHECK_TARGET(STD)
endif(NOT BRLCAD_IS_SUBBUILD)

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
# If options have been defined that we are going to document, by now they've
# been documented.  Make an updated copy of the "INSTALL" file and see whether
# anything has changed.
#
# Although the CMake configure process should leave the src dir pristine, the
# INSTALL and configure files present a problem in that they *do* need to be
# updated in the source tree.  As a compromise, what will happen when versions
# of these files are produced that are different from those found in the source
# tree the configure process will fail, with a message indicating that the user
# needs to move the newly generated version of the file(s) into the correct
# locations in the source tree.

set(CONFIG_FATAL_ERROR 0)

function(DIFF_FILE filename)
  file(READ "${BRLCAD_SOURCE_DIR}/${filename}" SRC_STR)
  file(READ "${BRLCAD_BINARY_DIR}/${filename}.new" BIN_STR)
  string(REGEX REPLACE "\r?\n" "" OLD_STR "${SRC_STR}")
  string(REGEX REPLACE "\r?\n" "" NEW_STR "${BIN_STR}")
  if(NOT "${OLD_STR}" STREQUAL "${NEW_STR}")
    set(CONFIG_FATAL_ERROR 1 PARENT_SCOPE)
    message("\nERROR: \"${BRLCAD_SOURCE_DIR}/${filename}\" is out of date.  An updated version has been generated at \"${BRLCAD_BINARY_DIR}/${filename}.new\"\nTo clear this, replace \"${BRLCAD_SOURCE_DIR}/${filename}\" with \"${BRLCAD_BINARY_DIR}/${filename}.new\" or find the source of the difference.\n")
  endif(NOT "${OLD_STR}" STREQUAL "${NEW_STR}")
endfunction(DIFF_FILE filename)

# Finalize and check INSTALL file
file(READ "${BRLCAD_SOURCE_DIR}/INSTALL" SRC_INSTALL_STR)
string(REGEX REPLACE "${CONFIG_OPT_STRING}.*" "" INSTALL_PREFIX "${SRC_INSTALL_STR}")
file(WRITE "${BRLCAD_BINARY_DIR}/INSTALL.new" "${INSTALL_PREFIX}")
file(READ "${BRLCAD_BINARY_DIR}/OPTIONS" INSTALL_OPTS)
file(APPEND "${BRLCAD_BINARY_DIR}/INSTALL.new" "${INSTALL_OPTS}")
file(APPEND "${BRLCAD_BINARY_DIR}/INSTALL.new" "\n\n*** Note - Do not add or edit configuration option descriptions and alias
   lists in this file - those entries are auto-generated from information in
   the toplevel CMakeLists.txt file and src/other/CMakeLists.txt - any changes
   should be made in those files.  The CMake configuration process will
   automatically re-generate INSTALL with the new descriptions and alias
   information.\n"
   )
DIFF_FILE(INSTALL)

# Do the same thing for the configure shell script - finish it and check
# for differences.
file(READ "${BRLCAD_CMAKE_DIR}/configure_suffix.sh" CONFIG_SUFFIX)
file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "${CONFIG_SUFFIX}")
DIFF_FILE(configure)

if(CONFIG_FATAL_ERROR)
  message(FATAL_ERROR "Configure haulted because INSTALL and/or configure script are out of date.")
endif(CONFIG_FATAL_ERROR)


# Because the build-time-delta needs a configure-file but comes at the
# end of the CMake configure, the preparation of the final distclean
# list must account for it.  Also, the distclean script itself must
# be added to the list, as it is generated by CMake
DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c")
get_property(CMAKE_DISTCLEAN_TARGET_LIST GLOBAL PROPERTY CMAKE_DISTCLEAN_TARGET_LIST)
list(REMOVE_DUPLICATES CMAKE_DISTCLEAN_TARGET_LIST)
configure_file("${BRLCAD_CMAKE_DIR}/distclean.cmake.in" "${BRLCAD_BINARY_DIR}/distclean.cmake" @ONLY)
DISTCLEAN("${BRLCAD_BINARY_DIR}/distclean.cmake")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake"
    )
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E echo "Running clean target..."
    COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E echo "Running clean target... done."
    COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake"
    )
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
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

#Done with all really time-consuming steps - do the configure time delta
if(NOT BRLCAD_IS_SUBBUILD)
  execute_process(COMMAND "${CMAKE_BINARY_DIR}/CMakeTmp/dreport" "Elapsed configuration time: " "${CONFIG_DELTA_START}")
endif(NOT BRLCAD_IS_SUBBUILD)

# Write out the Doxygen feature list
DOXYGEN_FEATURE_SUMMARY("${BRLCAD_BINARY_DIR}/CMakeTmp/features.dox")

add_custom_target(print-warning-message ${CMAKE_COMMAND} -E echo ""
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

# NOTE: set to NEW and remove slashes after 3.19 is required
if(POLICY CMP0110)
  cmake_policy(SET CMP0110 OLD)
endif(POLICY CMP0110)
add_test(NAME "NOTE:\\ some\\ 'test'\\ tests\\ are\\ expected\\ to\\ fail,\\ 'regress'\\ must\\ pass" COMMAND ${CMAKE_COMMAND} --build . --target print-warning-message)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
