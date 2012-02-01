#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2012 United States Government as represented by
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
#
# Early versions of this CMakeLists.txt file were based on the VTK
# CMakeLists.txt file, also licensed under Modified BSD.

# *******************************************************************
# ***                 BRL-CAD's CMakeLists.txt                    ***
# *******************************************************************
#
# This file defines the toplevel CMake build logic for BRL-CAD.
# As best as is reasonably possible, proper ordering and
# separation of tests and settings should be maintained per the
# recommended standard layout.  The tests should be added to the
# labeled sections below so that they are organized as follows:
#
#     0) information on the package and toplevel CMake settings
#     1) define top level options
#     2) check programs
#     3) check compiler characteristics
#     4) check libraries
#     5) check headers
#     6) check types/structures
#     7) check functions
#     8) check system services
#     9) output a summary
#
# The output summary should report key information about the final
# configuration of the build.  Comprehensive information is available
# in the CMake cache file in the build directory, so just hit the
# high points in the summary.


# *******************************************************************
# ***                 Top Level Settings                          ***
# *******************************************************************
# This file contains the top level CMakeLists.txt logic for the
# BRL-CAD software package.

# First step - stash the initial CMakeCache.txt file, if it exists, for subsequent
# comparison
if(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  configure_file(${CMAKE_BINARY_DIR}/CMakeCache.txt ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev COPY_ONLY)
endif(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.4)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  CMAKE_POLICY(SET CMP0007 OLD)
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION} VERSION_GREATER 2.8.3)
    CMAKE_POLICY(SET CMP0017 OLD)
  endif(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION} VERSION_GREATER 2.8.3)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
project(BRLCAD)

#---------------------------------------------------------------------
# Write out entries to populate a tm struct to be used for time deltas
# later
if(NOT IS_SUBBUILD)
  set(DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_START")
  configure_file(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/timedelta_start.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    ${CMAKE_BINARY_DIR}/CMakeTmp
    ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c
    OUTPUT_VARIABLE COMPILEMESSAGES)
endif(NOT IS_SUBBUILD)


#---------------------------------------------------------------------
# If it exists, load a BRL-CAD_CONFIG.GLOBAL file.  This allows advanced
# users to specify options they want to always use by default even when
# doing clean BRL-CAD builds without a CMakeCache.txt file - for example,
# to always enable all local libs the following line can be added to
# BRL-CAD_CONFIG.GLOBAL:
#
# set(BRLCAD_BUNDLED_LIBS "Bundled" CACHE STRING "Enable all local libs")


if(EXISTS ${BRLCAD_SOURCE_DIR}/../BRL-CAD_CONFIG.GLOBAL)
  message("Reading ${BRLCAD_SOURCE_DIR}/../BRL-CAD_CONFIG.GLOBAL")
  include(${BRLCAD_SOURCE_DIR}/../BRL-CAD_CONFIG.GLOBAL)
endif(EXISTS ${BRLCAD_SOURCE_DIR}/../BRL-CAD_CONFIG.GLOBAL)

# Allow the BRLCAD_ROOT environment variable to set CMAKE_INSTALL_PREFIX
# but be noisy about it.  This is generally not a good idea.
find_program(SLEEP_EXEC sleep)
mark_as_advanced(SLEEP_EXEC)
set(BRLCAD_ROOT "$ENV{BRLCAD_ROOT}")
if(BRLCAD_ROOT)
  if(NOT BRLCAD_ROOT_OVERRIDE)
    message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nBRLCAD_ROOT should only be used to override an install directory at runtime. BRLCAD_ROOT is presently set to \"${BRLCAD_ROOT}\"  It is *highly* recommended that BRLCAD_ROOT be unset and not used.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
    if(CMAKE_INSTALL_PREFIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
      if(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
	message("BRLCAD_ROOT is not necessary and may cause unexpected behavior")
      else(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
	message(FATAL_ERROR "BRLCAD_ROOT environment variable conflicts with CMAKE_INSTALL_PREFIX")
      endif(${BRLCAD_ROOT} STREQUAL "${CMAKE_INSTALL_PREFIX}")
    endif(CMAKE_INSTALL_PREFIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 2)
    endif(SLEEP_EXEC)
  endif(NOT BRLCAD_ROOT_OVERRIDE)
endif(BRLCAD_ROOT)
if(NOT BRLCAD_ROOT_OVERRIDE)
  if(BRLCAD_ROOT STREQUAL "/usr" OR ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
    message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nIt is STRONGLY recommended that you DO NOT install BRL-CAD into /usr as BRL-CAD provides several libraries that may conflict with other libraries (e.g. librt, libbu, libbn) on certain system configurations.\nSince our libraries predate all those that we're known to conflict with and are at the very core of our geometry services and project heritage, we have no plans to change the names of our libraries at this time.\nINSTALLING INTO /usr CAN MAKE A SYSTEM COMPLETELY UNUSABLE.  If you choose to continue installing into /usr, you do so entirely at your own risk.  You have been warned.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
    if(SLEEP_EXEC)
      message("Pausing 15 seconds...")
      execute_process(COMMAND ${SLEEP_EXEC} 15)
    endif(SLEEP_EXEC)
    if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
      message(FATAL_ERROR "If you wish to proceed using /usr as your prefix, define BRLCAD_ROOT_OVERRIDE=1 for CMake")
    endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
  endif(BRLCAD_ROOT STREQUAL "/usr" OR ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
endif(NOT BRLCAD_ROOT_OVERRIDE)

# While we're at it, complain if BRLCAD_DATA is set
set(BRLCAD_DATA_ENV "$ENV{BRLCAD_DATA}")
if(BRLCAD_DATA_ENV)
  message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nBRLCAD_DATA should only be used to override an install directory at runtime. BRLCAD_DATA is presently set to \"${BRLCAD_DATA_ENV}\"  It is *highly* recommended that BRLCAD_DATA be unset and not used.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 2)
  endif(SLEEP_EXEC)
endif(BRLCAD_DATA_ENV)

# Searching the system for packages presents something of a dilemma -
# in most situations it is Very Bad for a BRL-CAD build to be using
# older versions of libraries in install directories as search results.
# Generally, the desired behavior is to ignore whatever libraries are
# in the install directories, and only use external library results if
# they are something already found on the system due to non-BRL-CAD
# installation (source compile, package managers, etc.).  Unfortunately,
# CMake's standard behavior is to add a CMAKE_INSTALL_PREFIX to the
# search path once defined, resulting in (for us) an unexpected behavior
# of returning old installed libraries when CMake is re-run in a
# directory.
#
# To work around this, there are two possible approaches.  One,
# identified by Maik Beckmann, operates on CMAKE_SYSTEM_PREFIX_PATH:
#
# http://www.cmake.org/pipermail/cmake/2010-October/040292.html
#
# The other, pointed out by Michael Hertling, uses the
# CMake_[SYSTEM_]IGNORE_PATH variables.
#
# http://www.cmake.org/pipermail/cmake/2011-May/044503.html
#
# BRL-CAD initially operated on CMAKE_SYSTEM_PREFIX_PATH, but has
# switched to using the *_IGNORE_PATH variables.  This requires
# CMake 2.8.3 or later.
#
# The complication with ignoring install paths is if we are
# installing to a "legitimate" system search path - i.e. our
# CMAKE_INSTALL_PREFIX value is standard enough that it is a legitimate
# search target for find_package. In this case, we can't exclude
# accidental hits on our libraries without also excluding legitimate
# find_package results.  So the net results are:
#
# 1.  If you are planning to install to a system directory (typically
#     a bad idea but the settings are legal) clean out the old system
#     first or accept that the old libraries will be found and used.
#
# 2.  For more custom paths, the logic below will avoid the value
#     of CMAKE_INSTALL_PREFIX in find_package searches
#
# (Note:  CMAKE_INSTALL_PREFIX must be checked in the case where someone
# sets it on the command line prior to CMake being run.  BRLCAD_PREFIX
# preserves the CMAKE_INSTALL_PREFIX setting from the previous CMake run.
# CMAKE_INSTALL_PREFIX does not seem to be immediately set in this context
# when CMake is re-run unless specified explicitly on the command line.
# To ensure the previous (and internally set) CMAKE_INSTALL_PREFIX value
# is available, BRLCAD_PREFIX is used to store the value in the cache.)

if(CMAKE_INSTALL_PREFIX)
  if(NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr" AND NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED ${CMAKE_INSTALL_PREFIX}/lib ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH ${PATH_NORMALIZED})
  endif(NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr" AND NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/local")
endif(CMAKE_INSTALL_PREFIX)
if(BRLCAD_PREFIX)
  if(NOT ${BRLCAD_PREFIX} STREQUAL "/usr" AND NOT ${BRLCAD_PREFIX} STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED ${BRLCAD_PREFIX}/lib ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH ${PATH_NORMALIZED})
  endif(NOT ${BRLCAD_PREFIX} STREQUAL "/usr" AND NOT ${BRLCAD_PREFIX} STREQUAL "/usr/local")
endif(BRLCAD_PREFIX)
mark_as_advanced(CMAKE_SYSTEM_IGNORE_PATH) 

# Define CMake routines for bookkeeping of build files
include(${CMAKE_SOURCE_DIR}/misc/CMake/CMakeFiles.cmake)

#---------------------------------------------------------------------
# Define the current BRL-CAD version.
# See HACKING for details on how to properly update the version

file(READ ${BRLCAD_SOURCE_DIR}/include/conf/MAJOR BRLCAD_MAJOR_VERSION)
string(STRIP ${BRLCAD_MAJOR_VERSION} BRLCAD_MAJOR_VERSION)
file(READ ${BRLCAD_SOURCE_DIR}/include/conf/MINOR BRLCAD_MINOR_VERSION)
string(STRIP ${BRLCAD_MINOR_VERSION} BRLCAD_MINOR_VERSION)
file(READ ${BRLCAD_SOURCE_DIR}/include/conf/PATCH BRLCAD_PATCH_VERSION)
string(STRIP ${BRLCAD_PATCH_VERSION} BRLCAD_PATCH_VERSION)

set(BRLCAD_VERSION "${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")

#---------------------------------------------------------------------
# Define relative install locations.  Don't set these if they have already
# been set by some other means (like a higher level CMakeLists.txt file
# including this one.)

# The location in which to install BRLCAD executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# The location in which to install BRLCAD header files.
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

# The location in which to install BRL-CAD libraries.
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)

# The location in which to install BRL-CAD configuration files.
if(NOT CONF_DIR)
  set(CONF_DIR etc)
endif(NOT CONF_DIR)

# The location in which to install CMake scripts for packaging BRLCAD.
if(NOT PACKAGE_DIR)
  set(PACKAGE_DIR lib)
endif(NOT PACKAGE_DIR)

# The location in which to install BRL-CAD data files
if(NOT DATA_DIR)
  set(DATA_DIR "share/brlcad/${BRLCAD_VERSION}")
endif(NOT DATA_DIR)

# The location in which to install BRL-CAD Manual pages
if(NOT MAN_DIR)
  set(MAN_DIR ${DATA_DIR}/man)
endif(NOT MAN_DIR)

# The location in which to install BRL-CAD documentation files
if(NOT DOC_DIR)
  set(DOC_DIR ${DATA_DIR}/doc)
endif(NOT DOC_DIR)



#---------------------------------------------------------------------
# By default (as of version 2.8.2) CMake does not provide access to
# global lists of executable and library targets.  This is useful
# in a number of situations related to formulating custom rules and
# target dependency management.  To avoid the necessity of replacing
# add_library and add_executable calls with custom macros, override
# the function names and call the _add_* functions to access the CMake
# functionality previously available under the add_* functions. See
# http://www.cmake.org/pipermail/cmake/2010-September/039388.html

# To allow a hypothetical parent build to disable this mechanism and
# replace it, we wrap the whole show in an IF conditional.  To avoid
# the distcheck setup, the parent file should define the variable
# IS_SUBBUILD to ON.  Note that this also disables the
# liblib prefix check in add_library, making that the responsibility
# of the parent build as well, and disables the mechanism for ensuring
# that the timing code runs at the correct points during the build.

# We also need to provide bookkeeping logic here for the distribution
# verification or "distcheck" routines that will validate the state
# of the source tree against that expected and accounted for in the
# build files.  The global coverage needed for the purpose results in
# the add_library/add_executable command override mechanism having
# to serve two purposes at once; since we only override these functions
# once the logic for both jobs is intertwined below.

if(NOT IS_SUBBUILD)
  # Functions in CMake have local variable scope,
  # hence the use of properties to allow access to directory-specific
  # and global information scopes.
  define_property(GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST BRIEF_DOCS "libtarget list" FULL_DOCS "Library target list")
  define_property(GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST BRIEF_DOCS "exec target list" FULL_DOCS "Executable target list")
  define_property(GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST BRIEF_DOCS "custom target list" FULL_DOCS "Custom target list")
  mark_as_advanced(CMAKE_LIBRARY_TARGET_LIST)
  mark_as_advanced(CMAKE_EXEC_TARGET_LIST)
  mark_as_advanced(CMAKE_CUSTOM_TARGET_LIST)

  # Need build_files_list to be clear each CMake run	
  file(WRITE ${CMAKE_BINARY_DIR}/build_files_list.txt "")

  # Override and wrap add_library.  While we're at it, avoid doubling up
  # on the lib prefix for libraries if the target name is lib<target>
  function(add_library name)
    get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
    _add_library(${name} ${ARGN})
    CMAKEFILES(${ARGN})
    if(${name} MATCHES "^lib*")
      set_target_properties(${name} PROPERTIES PREFIX "")
    endif(${name} MATCHES "^lib*")
    set_property(GLOBAL APPEND PROPERTY CMAKE_LIBRARY_TARGET_LIST ${name})
  endfunction(add_library)

  # Override and wrap add_executable
  function(add_executable name)
    get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
    _add_executable(${name} ${ARGN})
    CMAKEFILES(${ARGN})
    set_property(GLOBAL APPEND PROPERTY CMAKE_EXEC_TARGET_LIST ${name})
  endfunction(add_executable)

  # Override and wrap add_custom_target
  function(add_custom_target name)
    get_property(CMAKE_CUSTOM_TARGET_LIST GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST)
    _add_custom_target(${name} ${ARGN})
    set_property(GLOBAL APPEND PROPERTY CMAKE_CUSTOM_TARGET_LIST ${name})
  endfunction(add_custom_target)

  # Override and wrap configure_file.  In the case of a configure_file, we'll
  # check that the file is part of the source tree and not itself a
  # generated file, but not reject full-path entries since there are quite a
  # few of them.
  function(configure_file file)
    _configure_file(${file} ${ARGN})
    GET_FILENAME_COMPONENT(ITEM_ABS ${file} ABSOLUTE)
    set(TEST_BUILD_PATH "${CMAKE_BINARY_DIR}/")
    set(TEST_SOURCE_PATH "${CMAKE_SOURCE_DIR}/")
    string(REGEX REPLACE "^${TEST_BUILD_PATH}" "" BUILD_DIR_TRIMMED "${ITEM_ABS}")
    if(${ITEM_ABS} STREQUAL ${BUILD_DIR_TRIMMED})
      string(REGEX REPLACE "^${TEST_SOURCE_PATH}" "" SOURCE_DIR_TRIMMED "${ITEM_ABS}")
      if(NOT ${ITEM_ABS} STREQUAL ${SOURCE_DIR_TRIMMED})
	file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${ITEM_ABS}\n")
      endif(NOT ${ITEM_ABS} STREQUAL ${SOURCE_DIR_TRIMMED})
    endif(${ITEM_ABS} STREQUAL ${BUILD_DIR_TRIMMED})
  endfunction(configure_file)
  
  # Override and wrap add_subdirctory.
  function(add_subdirectory name)
    _add_subdirectory(${name} ${ARGN})
    file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${name}\n")
    file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${name}/CMakeLists.txt\n")
  endfunction(add_subdirectory)
endif(NOT IS_SUBBUILD)

file(REMOVE ${CMAKE_BINARY_DIR}/cmakefiles.cmake)
file(REMOVE ${CMAKE_BINARY_DIR}/cmakedirs.cmake)

# We wrap the message function to also append ALL messages to CMakeOutput.log,
# as well as printing them on the console output.
if(NOT IS_SUBBUILD)
  function(message)
    _message(${ARGN})
    string(REGEX REPLACE ";" ": " msg "${ARGV}")
    file(APPEND ${BRLCAD_BINARY_DIR}/CMakeFiles/CMakeOutput.log "${msg}\n")
  endfunction(message)
endif(NOT IS_SUBBUILD)

#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
set(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
set(CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR};${CMAKE_MODULE_PATH}")

# Load utility macros that will be used throughout the BRL-CAD
# build logic
include(${BRLCAD_CMAKE_DIR}/BRLCAD_Options.cmake)
include(${BRLCAD_CMAKE_DIR}/BRLCAD_Util.cmake)

#---------------------------------------------------------------------
# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables
# and set them to "C" so things like date output are as expected.
set(_orig_lc_all      $ENV{LC_ALL})
set(_orig_lc_messages $ENV{LC_MESSAGES})
set(_orig_lang        $ENV{LANG})
if(_orig_lc_all)
  set(ENV{LC_ALL}      C)
endif(_orig_lc_all)
if(_orig_lc_messages)
  set(ENV{LC_MESSAGES} C)
endif(_orig_lc_messages)
if(_orig_lang)
  set(ENV{LANG}        C)
endif(_orig_lang)

#---------------------------------------------------------------------
# Unfortunately, CMake doesn't give you variables with current day,
# month, etc.  There are several possible approaches to this, but most
# (e.g. the date command) are not cross platform. We build a small C
# file which writes out the needed values to files in the build
# directory. Those files are then read and stripped by CMake.
set(CONFIG_TIME_DAY_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_DAY")
set(CONFIG_TIME_MONTH_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_MONTH")
set(CONFIG_TIME_YEAR_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_YEAR")
set(CONFIG_TIMESTAMP_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIMESTAMP")
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include/conf")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/time.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/time.c)
TRY_RUN(TIME_RESULT TIME_COMPILED
  ${CMAKE_BINARY_DIR}/CMakeTmp
  ${CMAKE_BINARY_DIR}/CMakeTmp/time.c
  OUTPUT_VARIABLE COMPILEMESSAGES)
if(TIME_RESULT MATCHES "^0$")
  file(READ ${CONFIG_TIME_DAY_FILE} CONFIG_DAY)
  string(STRIP ${CONFIG_DAY} CONFIG_DAY)
  file(READ ${CONFIG_TIME_MONTH_FILE} CONFIG_MONTH)
  string(STRIP ${CONFIG_MONTH} CONFIG_MONTH)
  file(READ ${CONFIG_TIME_YEAR_FILE} CONFIG_YEAR)
  string(STRIP ${CONFIG_YEAR} CONFIG_YEAR)
  set(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
else(TIME_RESULT MATCHES "^0$")
  message(FATAL_ERROR "Code to determine current date and time failed!\n")
endif(TIME_RESULT MATCHES "^0$")

#---------------------------------------------------------------------
# print out the title with a pretty box computed to wrap around
BOX_PRINT("*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***" "*")

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)

#---------------------------------------------------------------------
# In the case of CMake, we're usually doing one of several Build
# Types.  It's very rare to want to NOT specify a build type, so
# make it a little more work to do so.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE_MSG)
  message("CMake build type not set - performing Debug build.")
  message("* To use optimized settings intended for a package or system install,")
  message("  specify a Release build (on the command line, -DCMAKE_BUILD_TYPE=Release).")
  message("* To force CMake to not set build type, specify -DCMAKE_BUILD_TYPE=NONE")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
  set(CMAKE_BUILD_TYPE_MSG TRUE CACHE INTERNAL "Printed build type info message")
endif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE_MSG)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

if(CMAKE_BUILD_TYPE MATCHES "NONE")
  set(CMAKE_BUILD_TYPE "")
endif(CMAKE_BUILD_TYPE MATCHES "NONE")

#---------------------------------------------------------------------
# The location in which to install BRLCAD.  Only do this if
# CMAKE_INSTALL_PREFIX hasn't been set already, to try and allow
# parent builds (if any) some control.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
  if(NOT WIN32)
    if("${CMAKE_BUILD_TYPE}" MATCHES "Release")
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/rel-${BRLCAD_VERSION}")
    elseif("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}")
    else("${CMAKE_BUILD_TYPE}" MATCHES "Release")
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad")
    endif("${CMAKE_BUILD_TYPE}" MATCHES "Release")
  endif(NOT WIN32)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "BRL-CAD install prefix" FORCE)
  set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
set(BRLCAD_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "BRL-CAD install prefix")
mark_as_advanced(BRLCAD_PREFIX) 

# If we've got a Release build with a Debug path or vice versa, change it - that
# is probably the most "expected" behavior.
if("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
  set(CMAKE_INSTALL_PREFIX "/usr/brlcad/rel-${BRLCAD_VERSION}" CACHE STRING "BRL-CAD install prefix" FORCE)
endif("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
if("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")
  set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}" CACHE STRING "BRL-CAD install prefix" FORCE)
endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND ${CMAKE_INSTALL_PREFIX}	STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")


#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}:\$ORIGIN/../${LIB_DIR}")
endif(NOT APPLE)
# On OSX, we need to set INSTALL_NAME_DIR instead of RPATH
# http://www.cmake.org/cmake/help/cmake-2-8-docs.html#variable:CMAKE_INSTALL_NAME_DIR
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}")

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable
# files will be placed after building but prior to install.
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all libraries.")
endif(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building all archives.")
endif(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/${BIN_DIR} CACHE INTERNAL "Single output directory for building all executables.")
endif(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

# Take the hint from http://forum.openscenegraph.org/viewtopic.php?t=6926 and
# make all configurations output to the same directories.  This is necessary
# for allowing the bu_brlcad_data mechanisms to work in the build directory.
foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${CFG_TYPE}" CFG_TYPE)
  if(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} libraries.")
  endif(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
  if(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/${LIB_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} archives.")
  endif(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
  if(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/${BIN_DIR} CACHE INTERNAL "Single output directory for building ${CFG_TYPE} executables.")
  endif(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
endforeach()

#---------------------------------------------------------------------
# We will need a brlcad_config.h.in file to hold all the #cmakedefine
# statements, which will in turn be used to generate a brlcad_conf.h
# file.  In autotools this process is handled by autoheader - in the
# case of CMake we wrap the CHECK_* functions and the creation of the
# entry in the brlcad_config.h.in file into one step via a macro.  To
# run this macro, the config.h.in file must first be present, which
# we take care of here:

set(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")
file(WRITE ${CONFIG_H_FILE} "/**** Define statements for CMake ****/\n")
file(APPEND ${CONFIG_H_FILE} "#ifndef __CONFIG_H__\n")
file(APPEND ${CONFIG_H_FILE} "#define __CONFIG_H__\n")

# If we're building on Windows, start off with the config_win.h file
if(WIN32)
  file(APPEND ${CONFIG_H_FILE} "#include \"config_win_cmake.h\"\n")
endif(WIN32)

# Set up some of the define statements for path information and other basics
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE \"brlcad\"\n")
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"http://brlcad.org\"\n")
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"BRL-CAD\"\n")
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"BRL-CAD ${BRLCAD_VERSION}\"\n")
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"brlcad\"\n")
file(APPEND ${CONFIG_H_FILE} "#define BRLCAD_DATA \"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}\"\n")
file(APPEND ${CONFIG_H_FILE} "#define BRLCAD_ROOT \"${CMAKE_INSTALL_PREFIX}\"\n")
file(APPEND ${CONFIG_H_FILE} "#define PACKAGE_VERSION \"${BRLCAD_VERSION}\"\n")
file(APPEND ${CONFIG_H_FILE} "#define VERSION \"${BRLCAD_VERSION}\"\n")

# A variety of debugging messages in the code key off of the DEBUG
# definition - unlike the build flags, this is turned on and off based
# on the build type.  A "Release" build with debugging compiler flags
# will still not print debugging messages conditionalized on DEBUG.
if("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  file(APPEND ${CONFIG_H_FILE} "#define DEBUG 1\n")
endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug")

#----------------------------------------------------------------------
# Decide whether to do a 32 or a 64 bit build.

set(WORD_SIZE_LABEL "Compile as 32BIT or 64BIT?")
if(NOT BRLCAD_WORD_SIZE)
  set(BRLCAD_WORD_SIZE "AUTO" CACHE STRING WORD_SIZE_LABEL)
endif(NOT BRLCAD_WORD_SIZE)
set_property(CACHE BRLCAD_WORD_SIZE PROPERTY STRINGS AUTO 32BIT 64BIT)
string(TOUPPER "${BRLCAD_WORD_SIZE}" BRLCAD_WORD_SIZE_UPPER)
set(BRLCAD_WORD_SIZE "${BRLCAD_WORD_SIZE_UPPER}" CACHE STRING WORD_SIZE_LABEL FORCE)
if(NOT BRLCAD_WORD_SIZE MATCHES "AUTO" AND NOT BRLCAD_WORD_SIZE MATCHES "64BIT" AND NOT BRLCAD_WORD_SIZE MATCHES "32BIT")
  message(WARNING "Unknown value ${BRLCAD_WORD_SIZE} supplied for BRLCAD_WORD_SIZE - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, 32BIT and 64BIT")
  set(BRLCAD_WORD_SIZE "AUTO" CACHE STRING WORD_SIZE_LABEL FORCE)
endif(NOT BRLCAD_WORD_SIZE MATCHES "AUTO" AND NOT BRLCAD_WORD_SIZE MATCHES "64BIT" AND NOT BRLCAD_WORD_SIZE MATCHES "32BIT")

if(NOT CMAKE_SIZEOF_VOID_P)
  message(WARNING "CMAKE_SIZEOF_VOID_P is not defined - assuming 32 bit platform")
  set(CMAKE_SIZEOF_VOID_P 4)
endif(NOT CMAKE_SIZEOF_VOID_P)

if(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  if(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
    set(CMAKE_WORD_SIZE "64BIT")
    set(BRLCAD_WORD_SIZE "64BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
  else(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
    if(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
      set(CMAKE_WORD_SIZE "32BIT")
      set(BRLCAD_WORD_SIZE "32BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
    else(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
      if(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
	set(CMAKE_WORD_SIZE "16BIT")
	set(BRLCAD_WORD_SIZE "16BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
      else(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
	set(CMAKE_WORD_SIZE "8BIT")
	set(BRLCAD_WORD_SIZE "8BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
    endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
  endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
else(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  set(CMAKE_WORD_SIZE "${BRLCAD_WORD_SIZE}")
endif(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
file(APPEND  ${CONFIG_H_FILE} "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")

# Enable/disable 64-bit build settings for MSVC, which is apparently
# determined at the CMake generator level - need to override other
# settings if the compiler disagrees with them.
if(MSVC)
  if(CMAKE_CL_64)
    if(NOT ${CMAKE_WORD_SIZE} MATCHES "64BIT")
      set(CMAKE_WORD_SIZE "64BIT")
      if(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
	message(WARNING "Selected MSVC compiler is 64BIT - setting word size to 64BIT.  To perform a 32BIT MSVC build, select the 32BIT MSVC CMake generator.")
	set(BRLCAD_WORD_SIZE "64BIT" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
    endif(NOT ${BRLCAD_WORD_SIZE} MATCHES "64BIT")
  else(CMAKE_CL_64)
    if(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
      set(CMAKE_WORD_SIZE "32BIT")
      if(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
	message(WARNING "Selected MSVC compiler is 32BIT - setting word size to 32BIT.  To perform a 64BIT MSVC build, select the 64BIT MSVC CMake generator.")
	set(BRLCAD_WORD_SIZE "32BIT" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
    endif(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
  endif(CMAKE_CL_64)
endif(MSVC)

# Based on what we are doing, we may need to constrain our search paths
if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
else(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")

# One of the problems with 32/64 building is we need to search anew
# for 64 bit libs after a 32 bit configure, or vice versa.
if(PREVIOUS_CONFIGURE_TYPE)
  if(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
    include(${CMAKE_SOURCE_DIR}/misc/CMake/ResetCache.cmake)
    RESET_CACHE_file()
  endif(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
endif(PREVIOUS_CONFIGURE_TYPE)

set(PREVIOUS_CONFIGURE_TYPE ${CMAKE_WORD_SIZE} CACHE STRING "Previous configuration word size" FORCE)
mark_as_advanced(PREVIOUS_CONFIGURE_TYPE)


# *******************************************************************
# ***        Top Level Configure Options - Stage 1 of 9           ***
# *******************************************************************
#
# Now we define the various options for BRL-CAD - ways to enable and
# disable features, select which parts of the system to build, etc.
# As much as possible, sane default options are either selected or
# detected.  Because documentation is autogenerated for BRL-CAD
# options, be sure to initialize the file.
set(CONFIG_OPT_STRING "CONFIGURATION OPTIONS\n---------------------\n")
file(WRITE ${CMAKE_BINARY_DIR}/OPTIONS "${CONFIG_OPT_STRING}")

# The BRL-CAD CMake build will also generate a configure script
# that emulates the command option style of GNU Autotool's 
# configure.  Write the pre-defined header into the build-dir template
# to initialize the file.
file(REMOVE ${CMAKE_BINARY_DIR}/configure.new)
file(READ ${BRLCAD_SOURCE_DIR}/misc/CMake/configure_prefix.sh CONFIG_PREFIX)
file(WRITE ${CMAKE_BINARY_DIR}/configure.new.tmp "${CONFIG_PREFIX}")
file(COPY ${CMAKE_BINARY_DIR}/configure.new.tmp DESTINATION
  ${CMAKE_BINARY_DIR}/CMakeFiles FILE_PERMISSIONS OWNER_READ OWNER_WRITE
  OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(REMOVE ${CMAKE_BINARY_DIR}/configure.new.tmp)
file(RENAME ${CMAKE_BINARY_DIR}/CMakeFiles/configure.new.tmp
  ${CMAKE_BINARY_DIR}/configure.new)

# Build shared libs by default.  Mark this as advanced - turning off
# ALL shared library building is unlikely to result in a working build
# and is not a typical configuration.  Note that turning this option off
# will not disable libraries specifically added as SHARED.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
mark_as_advanced(BUILD_SHARED_LIBS)

# Build static libs by default unless we're debugging. Note: this
# option will not disable libraries specifically added as STATIC even
# when OFF.
AUTO_option(BRLCAD_BUILD_STATIC_LIBS BUILD_STATIC_LIBS "OFF" "ON")


# On Mac OS X, it is common to have third party package managers 
# present for easy software installation (currently we're aware of
# Fink and MacPorts).  This can seriously complicate find_* results,
# so provide an option to specify whether or which of the third
# party setup to use.
include(${CMAKE_SOURCE_DIR}/misc/CMake/Fink_MacPorts.cmake)

# Turn on librtserver build.
option(BRLCAD_ENABLE_RTSERVER "Enable the librtserver target." ON)

# Turn off the brlcad.dll build.
# It's an expert's setting at the moment.
option(BRLCAD_ENABLE_BRLCAD_LIBRARY "Build the brlcad.dll" OFF)
mark_as_advanced(BRLCAD_ENABLE_BRLCAD_LIBRARY)

# Global third party controls - these options enable and disable ALL
# local copies of libraries in src/other.  Forcing all local
# libraries off is not usually recommended unless attempting to
# build packages for a distribution.  If both of these options are
# on the enabling of local packages is the "senior" option and will
# force the system libs option to off.
set(BRLCAD_BUNDLED_LIBS_ALIASES "ENABLE_ALL")
set(BRLCAD_BUNDLED_LIBS_DESCRIPTION "
Enables compilation of all 3rd party sources that are provided within a BRL-CAD
source distribution.  If used this option sets all other 3rd party library 
build flags to ON by default.  However, that setting can be overridden by 
manually setting individual variables. Default is \"AUTO\" - 3rd party sources 
are compiled only if they are not detected as being available and functioning 
as expected.
")
BRLCAD_option(AUTO BRLCAD_BUNDLED_LIBS BRLCAD_BUNDLED_LIBS_ALIASES BRLCAD_BUNDLED_LIBS_DESCRIPTION)
set_property(CACHE BRLCAD_BUNDLED_LIBS PROPERTY STRINGS AUTO BUNDLED SYSTEM)
string(TOUPPER "${BRLCAD_BUNDLED_LIBS}" BRLCAD_BUNDLED_LIBS_UPPER)
set(BRLCAD_BUNDLED_LIBS "${BRLCAD_BUNDLED_LIBS_UPPER}" CACHE STRING "Build bundled libraries." FORCE)
if(${BRLCAD_BUNDLED_LIBS} MATCHES "ON")
  set(BRLCAD_BUNDLED_LIBS "BUNDLED" CACHE STRING "Build bundled libraries." FORCE)
endif(${BRLCAD_BUNDLED_LIBS} MATCHES "ON")
if(${BRLCAD_BUNDLED_LIBS} MATCHES "OFF")
  set(BRLCAD_BUNDLED_LIBS "SYSTEM" CACHE STRING "Build bundled libraries." FORCE)
endif(${BRLCAD_BUNDLED_LIBS} MATCHES "OFF")
if(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")
  message(WARNING "Unknown value BRLCAD_BUNDLED_LIBS supplied for BRLCAD_BUNDLED_LIBS - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, BUNDLED and SYSTEM")
  set(BRLCAD_BUNDLED_LIBS "AUTO" CACHE STRING "Build	bundled libraries." FORCE)
endif(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building. Not currently working - needs work in at least Tk and togl
# CMake logic (probably more), and the display manager/framebuffer codes are known to depend
# on either GLX or WGL specifically in their current forms.
option(BRLCAD_ENABLE_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)
mark_as_advanced(BRLCAD_ENABLE_AQUA)

# Install example BRL-CAD Geometry Files
option(BRLCAD_INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)

# Enable features requiring X11
if(WIN32)
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
  mark_as_advanced(BRLCAD_ENABLE_X11)
elseif(BRLCAD_ENABLE_AQUA)
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
else(WIN32)
  option(BRLCAD_ENABLE_X11 "Use X11." ON)
endif(WIN32)
if(BRLCAD_ENABLE_X11)
  set(BRLCAD_ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
  set(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif(BRLCAD_ENABLE_X11)
mark_as_advanced(OPENGL_USE_AQUA)

# Enable/disable features requiring the Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tk
option(BRLCAD_ENABLE_TK "Enable features requiring the Tk toolkit" ON)
mark_as_advanced(BRLCAD_ENABLE_TK)
if(NOT WIN32)
  if(APPLE)
    if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA)
      set(BRLCAD_ENABLE_TK OFF)
    endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA)
    if(BRLCAD_ENABLE_X11)
      set(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
    endif(BRLCAD_ENABLE_X11)
  else(APPLE)
    if(NOT BRLCAD_ENABLE_X11)
      set(BRLCAD_ENABLE_TK OFF)
    else(NOT BRLCAD_ENABLE_X11)
      set(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
    endif(NOT BRLCAD_ENABLE_X11)
  endif(APPLE)
endif(NOT WIN32)

# Enable features requring OPENGL
# Be smart about this - if we don't have X11 or Aqua and we're
# not on Windows, we're non-graphical and that means OpenGL is
# a no-go.  The Windows version would have to be some sort of
# option for the WIN32 graphics layer?  Should probably think
# about that... for now, on Win32 don't try OpenGL if Tk is 
# off.  That'll hold until we get a non-Tk based GUI - then
# setting non-graphical on Windows will take more thought.
if(NOT WIN32)
  if(APPLE)
    if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA)
      set(OPENGL_FOUND OFF)
      set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA" FORCE)
    else(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA)
      include(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
    endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA)
  else(APPLE)
    if(NOT BRLCAD_ENABLE_X11)
      set(OPENGL_FOUND OFF)
      set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - NOT BRLCAD_ENABLE_X11" FORCE)
    else(NOT BRLCAD_ENABLE_X11)
      include(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
    endif(NOT BRLCAD_ENABLE_X11)
  endif(APPLE)
else(NOT WIN32)
  if(BRLCAD_ENABLE_TK)
    include(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
  else(BRLCAD_ENABLE_TK)
    set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - WIN32 AND NOT BRLCAD_ENABLE_TK" FORCE)
  endif(BRLCAD_ENABLE_TK)
endif(NOT WIN32)

set(BRLCAD_ENABLE_OPENGL_ALIASES
  ENABLE_OPENGL
  )
set(BRLCAD_ENABLE_OPENGL_DESCRIPTION "
Enable support for OpenGL based Display Managers in BRL-CAD.
Default depends on whether OpenGL is successfully detected - 
if it is, default is to enable.
")
BRLCAD_option(${OPENGL_FOUND} BRLCAD_ENABLE_OPENGL	BRLCAD_ENABLE_OPENGL_ALIASES BRLCAD_ENABLE_OPENGL_DESCRIPTION)

# Enable RTGL.  Requires an enabled OpenGL.
option(BRLCAD_ENABLE_RTGL "Enable experimental RTGL code." OFF)
mark_as_advanced(BRLCAD_ENABLE_RTGL)
if(NOT BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_RTGL)
  message("RTGL requested, but OpenGL is not enabled - disabling")
  set(BRLCAD_ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
endif(NOT BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_RTGL)
if(NOT BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_RTGL)
  message("RTGL currently works only with GLX, and X11 is not enabled - disabling")
  set(BRLCAD_ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
endif(NOT BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_RTGL)
if(BRLCAD_ENABLE_AQUA)
  set(OPENGL_USE_AQUA ON CACHE STRING "Aqua enabled - use Aqua OpenGL" FORCE)
endif(BRLCAD_ENABLE_AQUA)


# Enable JOVE.
option(BRLCAD_ENABLE_JOVE "Enable jove editor build." OFF)
mark_as_advanced(BRLCAD_ENABLE_JOVE)


#----------------------------------------------------------------------
# There are extra documentation files available requiring DocBook
# They are quite useful in graphical interfaces, but also add considerably
# to the overall build time.  If necesssary BRL-CAD  provides its own 
# xsltproc (see src/other/xmltools), so the html and man page
# outputs are always potentially available.  PDF output, on the other hand,
# needs Apache FOP.  FOP is not a candidate to bundle with BRL-CAD for 
# a number of reasons, so we simply check to see if it is present and set 
# the options accordingly.

find_program(APACHE_FOP fop DOC "path to the exec script for Apache FOP")
mark_as_advanced(APACHE_FOP)
# We care about the FOP version, unfortunately - fine out what we've got.
if(APACHE_FOP)
  execute_process(COMMAND ${APACHE_FOP} -v OUTPUT_VARIABLE APACHE_FOP_INFO ERROR_QUIET)
  string(REGEX REPLACE "FOP Version ([0-9\\.]*)" "\\1" APACHE_FOP_VERSION "${APACHE_FOP_INFO}")
endif(APACHE_FOP)

# The HTML output is used in the graphical help browsers in MGED and Archer,
# as well as being the most likely candidate for external viewers. Turn this
# on unless explicitly instructed otherwise by the user.
set(BRLCAD_EXTRADOCS_ALIASES
  ENABLE_DOCS
  ENABLE_EXTRA_DOCS
  ENABLE_DOCBOOK
  )
set(BRLCAD_EXTRADOCS_DESCRIPTION "
The core option that enables and disables bulding of BRL-CAD's DocBook
based documentation (includes manuals and man pages for commands, among
other things.)  Defaults to ON, but only HTML and MAN formats are enabled
by default - PDF must be enabled separately.
")
BRLCAD_option(ON BRLCAD_EXTRADOCS BRLCAD_EXTRADOCS_ALIASES BRLCAD_EXTRADOCS_DESCRIPTION)

include(CMakeDependentOption)
# By default, always do HTML output if we're doing extradocs output
CMAKE_DEPENDENT_option(BRLCAD_EXTRADOCS_HTML "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_HTML)

# Normally, we'll turn on man page output by default, but there is
# no point in doing man page output for a Visual Studio build - the
# files aren't useful and it *seriously* increases the target build 
# count/build time.  Conditionalize on the CMake MSVC variable NOT 
# being set.
CMAKE_DEPENDENT_option(BRLCAD_EXTRADOCS_MAN "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS; NOT MSVC" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_MAN)

# Don't do PDF by default, and hide the option unless the tools to do it are
# present.  Provide an option to disable the PDF output for man pages without
# disabling the rest of the PDF output - PDF generation for hundreds of man
# pages can be expensive in time.
CMAKE_DEPENDENT_option(BRLCAD_EXTRADOCS_PDF "Build PDF output from DocBook documentation" OFF "BRLCAD_EXTRADOCS;APACHE_FOP" OFF)
CMAKE_DEPENDENT_option(BRLCAD_EXTRADOCS_PDF_MAN "Build PDF output from DocBook documentation for man pages" ON "BRLCAD_EXTRADOCS_PDF" OFF)

# Don't do it by default, but provide an option to perform validation as part
# of the DocBook build - sort of a "strict flags" mode for DocBook
CMAKE_DEPENDENT_option(BRLCAD_EXTRADOCS_VALIDATE "Perform validation for DocBook documentation" OFF "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_VALIDATE)

#----------------------------------------------------------------------
# The following are fine-grained options for enabling/disabling compiler
# and source code definition settings.  Typically these are set to
# various configurations by the toplevel CMAKE_BUILD_TYPE setting, but
# can also be individually set.

# Enable/disable runtime debugging - these are protections for
# minimizing the possibility of corrupted data files.  Generally
# speaking these should be left on.
set(BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES 
  ENABLE_RUNTIME_DEBUG
  ENABLE_RUN_TIME_DEBUG
  ENABLE_RUNTIME_DEBUGGING
  ENABLE_RUN_TIME_DEBUGGING)
set(BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION "
Enables support for application and library debugging facilities.  
Disabling the run-time debugging facilities can provide a significant 
(10%-30%) performance boost at the expense of extensive error 
checking (that in turn help prevent corruption of your data).  
Default is "ON", and should only be disabled for read-only render 
work where performance is critical.
")
BRLCAD_option(ON BRLCAD_ENABLE_RUNTIME_DEBUG BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_RUNTIME_DEBUG)
if(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)
  message("}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  message("While disabling run-time debugging should increase")
  message("performance, it will likewise remove several")
  message("data-protection safeguards that are in place to")
  message("minimize the possibility of corrupted data files")
  message("in the inevitable event of a user encountering a bug.")
  message("You have been warned.  Proceed at your own risk.")
  message("{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{")
  file(APPEND  ${CONFIG_H_FILE} "/*Define to not do anything for macros that only bomb on a fatal error. */\n")
  file(APPEND  ${CONFIG_H_FILE} "#define NO_BOMBING_MACROS 1\n")
  file(APPEND  ${CONFIG_H_FILE} "/*Define to not perform magic number checking */\n")
  file(APPEND  ${CONFIG_H_FILE} "#define NO_MAGIC_CHECKING 1\n")
  file(APPEND  ${CONFIG_H_FILE} "/*Define to not check for divide by zero during ray shots */\n")
  file(APPEND  ${CONFIG_H_FILE} "#define NO_BADRAY_CHECKING 1\n")
  file(APPEND  ${CONFIG_H_FILE} "/*Define to not provide run-time debug facilities via rt_g.debug */\n")
  file(APPEND  ${CONFIG_H_FILE} "#define NO_DEBUG_CHECKING 1\n")
endif(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)

# Enable debug flags during compilation
set(BRLCAD_FLAGS_DEBUG_ALIASES
  ENABLE_DEBUG
  ENABLE_FLAGS_DEBUG
  ENABLE_DEBUG_FLAGS
  )
set(BRLCAD_FLAGS_DEBUG_DESCRIPTION "
Add compiler flags to aid in program debugging.  Default depends on CMake
build type - defaults to ON for Debug, OFF for Release.
")
AUTO_option(BRLCAD_FLAGS_DEBUG BRLCAD_DEBUG_BUILD "ON" "OFF")
OPTION_ALIASES(BRLCAD_FLAGS_DEBUG BRLCAD_FLAGS_DEBUG_ALIASES "BOOL")
OPTION_DESCRIPTION(BRLCAD_FLAGS_DEBUG BRLCAD_FLAGS_DEBUG_ALIASES BRLCAD_FLAGS_DEBUG_DESCRIPTION)

# Build with compiler warning flags
set(BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  ENABLE_WARNINGS
  ENABLE_COMPILER_WARNINGS
  )
set(BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION "
Use extra compiler warning flags when compiling C/C++ code.  Defaults to ON.
")
BRLCAD_option(ON BRLCAD_ENABLE_COMPILER_WARNINGS
  BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_COMPILER_WARNINGS)

# Enable/disable strict compiler settings - these are limited to libraries that
# specifically inform the BRLCAD_ADDLIB macro they can be built with STRICT flags.

set(BRLCAD_ENABLE_STRICT_ALIASES 
  ENABLE_STRICT
  ENABLE_STRICT_COMPILE
  ENABLE_STRICT_COMPILE_FLAGS)
set(BRLCAD_ENABLE_STRICT_DESCRIPTION "
Causes all compilation warnings for C code to be treated as errors.  This is now
the default for BRL-CAD source code, and developers should address issues
discovered by these flags whenever possible rather than disabling strict
mode.
")
BRLCAD_option(ON BRLCAD_ENABLE_STRICT BRLCAD_ENABLE_STRICT_ALIASES BRLCAD_ENABLE_STRICT_DESCRIPTION)

# The full BRL-CAD strict flags are too much for a variety of system
# C++ headers presently (Dec. 2011) so we don't default to erroring out
# on warnings for C++
option(BRLCAD_ENABLE_CXX_STRICT "Use strict compiler settings on cxx files in libraries that build with strict" OFF)
if(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_CXX_STRICT)
  file(APPEND  ${CONFIG_H_FILE} "#define STRICT_FLAGS 1\n")
endif(BRLCAD_ENABLE_STRICT)

# Build with compiler optimization flags.  This should normally be on for release builds
AUTO_option(BRLCAD_FLAGS_OPTIMIZATION BRLCAD_OPTIMIZED_BUILD "OFF" "ON")

# By default, we don't want any error reports at all from src/other
option(BRLCAD_DISABLE_SRC_OTHER_WARN "Disable warnings for src/other" ON)
mark_as_advanced(BRLCAD_DISABLE_SRC_OTHER_WARN)

# Build with full compiler lines visible by default (won't need make
# VERBOSE=1) on command line
option(BRLCAD_ENABLE_VERBOSE_PROGRESS "verbose output" OFF)
mark_as_advanced(BRLCAD_ENABLE_VERBOSE_PROGRESS)
if(BRLCAD_ENABLE_VERBOSE_PROGRESS)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(BRLCAD_ENABLE_VERBOSE_PROGRESS)

# Build with profiling support
option(BRLCAD_ENABLE_PROFILING "Build with profiling support" OFF)
mark_as_advanced(BRLCAD_ENABLE_PROFILING)

# Build with dtrace support
option(BRLCAD_ENABLE_DTRACE "Build with dtrace support" OFF)
mark_as_advanced(BRLCAD_ENABLE_DTRACE)
if(BRLCAD_ENABLE_DTRACE)
  BRLCAD_INCLUDE_FILE(sys/sdt.h HAVE_SYS_SDT_H)
  if(NOT HAVE_SYS_SDT_H)
    set(BRLCAD_ENABLE_DTRACE OFF)
  endif(NOT HAVE_SYS_SDT_H)
endif(BRLCAD_ENABLE_DTRACE)

# Take advantage of parallel processors if available - highly recommended
option(BRLCAD_ENABLE_SMP "Enable SMP architecture parallel computation support" ON)
if(WIN32 AND BRLCAD_ENABLE_SMP)
  message("SMP Support is not ready on Windows - disabling")
  set(BRLCAD_ENABLE_SMP OFF)
endif(WIN32 AND BRLCAD_ENABLE_SMP)
mark_as_advanced(BRLCAD_ENABLE_SMP)
if(BRLCAD_ENABLE_SMP)
  file(APPEND  ${CONFIG_H_FILE} "#define PARALLEL 1\n")
endif(BRLCAD_ENABLE_SMP)

# *******************************************************************
# ***            Check for Programs - Stage 2 of 9                ***
# *******************************************************************

# A variety of tools, such as the benchmark utilities, need
# a Bourne shell and other commands - check for them.
include(${BRLCAD_CMAKE_DIR}/FindShellDeps.cmake)

# CMake can detect flex and bison, but apparently not generic lex/yacc.
# Solution is to make our own generic lex/yacc Find routines that look
# first for flex/bison, but will take whatever's available.  Since the
# BRL-CAD project isn't supposed to rely on flex/bison specific features
# anyway, there shouldn't be a problem.
include(${BRLCAD_CMAKE_DIR}/FindLEX.cmake)
include(${BRLCAD_CMAKE_DIR}/FindYACC.cmake)


# If using dtrace, we will need to find it
if(BRLCAD_ENABLE_DTRACE)
  find_program(DTRACE_EXEC NAMES dtrace DOC "path to dtrace executable")
endif(BRLCAD_ENABLE_DTRACE)


# Load various wrapper macros for checking libraries, headers and
# functions
include(${BRLCAD_CMAKE_DIR}/BRLCAD_CheckFunctions.cmake)


# *******************************************************************
# ***      Check for Compiler Characteristics - Stage 3 of 9      ***
# *******************************************************************

# at this stage only the gcc compiler is supported
if(NOT MSVC)
  include(${BRLCAD_CMAKE_DIR}/CompilerFlags.cmake)
endif(NOT MSVC)

# The src/other subdirectory does not share (at least, not universally)
# certain definitions common to all other directories in BRL-CAD.  Also,
# we don't want flags that are otherwise global to BRL-CAD to be passed
# to the src/other directory (particularly things like strict compiler
# flags) so add src/other before any of that setup occurs.
add_subdirectory(src/other)

# Requires that CompilerFlags.cmake already be loaded
if(NOT MSVC)
  include(${BRLCAD_CMAKE_DIR}/BRLCAD_CompilerFlags.cmake)
endif(NOT MSVC)

# If doing a debug build, set _FORTIFY_SOURCE to 2. Provides
# compile-time best-practice error checking on certain libc
# functions (e.g., memcpy), and provides run-time checks on buffer
# lengths and memory regions.
if(${BRLCAD_DEBUG_BUILD} MATCHES "ON")
  file(APPEND  ${CONFIG_H_FILE} "#define _FORTIFY_SOURCE 2\n")
endif(${BRLCAD_DEBUG_BUILD} MATCHES "ON")

#CHECK_C_COMPILER_FLAG("Wl,--no-undefined" NO_UNDEFINED_LINKER_FLAG)


# *******************************************************************
# ***            Check for Libraries - Stage 4 of 9               ***
# *******************************************************************

# While the primary purpose of this section is to identify libraries,
# some of the headers we are looking for are associated with the
# libraries checked here.  In those cases, we will handle the header
# logic here as opposed to separating the header logic from the
# find_package call.

# TODO - need to make LINKOPT vars for anything here that will
# be referenced in a pkgconfig file

# Look for threads (doesn't check for headers)
# The variable CMAKE_THREAD_LIBS_INIT is the one of interest
# when writing target_link_libraries lists.
find_package(Threads)
BRLCAD_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)

# Check for the daemon function in -lbsd and/or -lc for adrt
BRLCAD_CHECK_LIBRARY(BSD bsd daemon)
BRLCAD_CHECK_LIBRARY(BSD c daemon)

# Check for CoreFoundation, Carbon and Cocoa on Apple
if(APPLE)
  include(CMakeFindFrameworks)
  CMAKE_FIND_FRAMEWORKS(CoreFoundation)
  if(CoreFoundation_FRAMEWORKS)
    set(CoreFoundation_LIBRARIES "-framework CoreFoundation" CACHE FILEPATH "CoreFoundation framework" FORCE)
  endif(CoreFoundation_FRAMEWORKS)
  CMAKE_FIND_FRAMEWORKS(Carbon)
  if(Carbon_FRAMEWORKS)
    set(Carbon_LIBRARIES "-framework Carbon" CACHE FILEPATH "Carbon framework" FORCE)
  endif(Carbon_FRAMEWORKS)
  BRLCAD_INCLUDE_FILE(Carbon/Carbon.h HAVE_CARBON_CARBON_H)
  CMAKE_FIND_FRAMEWORKS(Cocoa)
  if(Cocoa_FRAMEWORKS)
    set(Cocoa_LIBRARIES "-framework Cocoa" CACHE FILEPATH "Cocoa framework" FORCE)
  endif(Cocoa_FRAMEWORKS)
  mark_as_advanced(CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
  mark_as_advanced(CoreFoundation_LIBRARIES Carbon_LIBRARIES Cocoa_LIBRARIES)
endif(APPLE)

# crypt library (needed on some platforms for enigma)
FIND_LIBRARY(CRYPT_LIBRARY NAMES crypt)
mark_as_advanced(CRYPT_LIBRARY)

# Find Java and JNI - conditionalize the include settings on
# the success of the find_package calls.
FIND_PACKAGE(Java)
FIND_PACKAGE(JNI)
if(JNI_FOUND AND JAVA_FOUND)
  BRLCAD_INCLUDE_FILE(JavaVM/jni.h HAVE_JAVAVM_JNI_H)
  BRLCAD_INCLUDE_FILE(jni.h HAVE_JNI_H)
  if(NOT HAVE_JNI_H AND NOT HAVE_JAVAVM_JNI_H)
    set(BRLCAD_ENABLE_RTSERVER OFF CACHE BOOL "RT server disabled due to	unsatisfied Java requirements" FORCE)
    message("librtserver build requested, but JNI header not found - disabling")
  endif(NOT HAVE_JNI_H AND NOT HAVE_JAVAVM_JNI_H)
else(JNI_FOUND AND JAVA_FOUND)
  set(BRLCAD_ENABLE_RTSERVER OFF CACHE BOOL "RT server disabled due to	unsatisfied Java requirements" FORCE)
  message("librtserver build requested, but Java+JNI not found - disabling")
endif(JNI_FOUND AND JAVA_FOUND)
mark_as_advanced(BRLCAD_ENABLE_RTSERVER)

# Find X11 if it is enabled
if(BRLCAD_ENABLE_X11)
  include(${BRLCAD_CMAKE_DIR}/FindX11.cmake)
  BRLCAD_INCLUDE_FILE(X11/Xlib.h HAVE_X11_XLIB_H)
endif(BRLCAD_ENABLE_X11)

# On SunOS with Sun Studio we need sunmath
CHECK_LIBRARY_EXISTS(sunmath ieee_flags "" HAVE_SUNMATH_LIBRARY)
if(HAVE_SUNMATH_LIBRARY)
  set(SUN_MATH_LIB "-lsunmath")
endif(HAVE_SUNMATH_LIBRARY)

# m library is not needed on Windows
if(NOT WIN32)
  BRLCAD_CHECK_LIBRARY(M m cos)
  mark_as_advanced(M_LIBRARY)
endif(NOT WIN32)

# socket library
BRLCAD_CHECK_LIBRARY(SOCKET socket socket)

# network socket library
BRLCAD_CHECK_LIBRARY(NS nsl gethostbyaddr)

# network library
BRLCAD_CHECK_LIBRARY(NETWORK_SOCKET network socket)

# malloc library
BRLCAD_CHECK_LIBRARY(MALLOC c mallopt)
BRLCAD_CHECK_LIBRARY(MALLOC malloc mallopt)

# mx library
BRLCAD_CHECK_LIBRARY(MX mx matherr)

# dynamic link library
BRLCAD_CHECK_LIBRARY(DLOPEN dl dlopen)

# SGI generic SCI library
BRLCAD_CHECK_LIBRARY(DS ds dsopen)

# OpenGL graphics
if(BRLCAD_ENABLE_OPENGL)
  if(OPENGL_INCLUDE_DIR_GL)
    file(APPEND  ${CONFIG_H_FILE} "#define HAVE_GL_GL_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GL)
  if(OPENGL_INCLUDE_DIR_GLX)
    file(APPEND  ${CONFIG_H_FILE} "#define HAVE_GL_GLX_H 1\n")
  endif(OPENGL_INCLUDE_DIR_GLX)
endif(BRLCAD_ENABLE_OPENGL)

# SGI graphics library
BRLCAD_CHECK_LIBRARY(SGIGL gl gconfig)

# Solaris lexer library
BRLCAD_CHECK_LIBRARY(SOLARIS_LEXER l yyless)

# IRIX libgen library
BRLCAD_CHECK_LIBRARY(GEN gen basename)

# Bullet physics library
FIND_PACKAGE(Bullet)
if(BULLET_FOUND)
  file(APPEND ${CONFIG_H_FILE} "#define HAVE_BULLET 1\n")
endif(BULLET_FOUND)
mark_as_advanced(BULLET_INCLUDE_DIR)


# *******************************************************************
# ***             Check for Headers - Stage 5 of 9                ***
# *******************************************************************

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
file(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")

# AC_HEADER_SYS_WAIT
BRLCAD_HEADER_SYS_WAIT()
# dirent.h is POSIX.1, but not present on Windows (grr)
# so we need to check for it
BRLCAD_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)

# Need to compare this with the configure.ac list
BRLCAD_INCLUDE_FILE(arpa/inet.h HAVE_ARPA_INET_H)
BRLCAD_INCLUDE_FILE(curses.h HAVE_CURSES_H)
BRLCAD_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
BRLCAD_INCLUDE_FILE(dslib.h HAVE_DSLIB_H)
BRLCAD_INCLUDE_FILE(getopt.h HAVE_GETOPT_H)
BRLCAD_INCLUDE_FILE(gl/device.h HAVE_GL_DEVICE_H)
BRLCAD_INCLUDE_FILE(gl/glext.h HAVE_GL_GLEXT_H)
BRLCAD_INCLUDE_FILE(gl/wglext.h HAVE_GL_WGLEXT_H)
BRLCAD_INCLUDE_FILE(grp.h HAVE_GRP_H)
BRLCAD_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
BRLCAD_INCLUDE_FILE(io.h HAVE_IO_H)
BRLCAD_INCLUDE_FILE(libgen.h HAVE_LIBGEN_H)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(ncurses.h HAVE_NCURSES_H)
BRLCAD_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
BRLCAD_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
BRLCAD_INCLUDE_FILE(poll.h HAVE_POLL_H)
BRLCAD_INCLUDE_FILE(pwd.h HAVE_PWD_H)
BRLCAD_INCLUDE_FILE(rle.h HAVE_RLE_H)
BRLCAD_INCLUDE_FILE(sched.h HAVE_SCHED_H)
BRLCAD_INCLUDE_FILE(sgtty.h HAVE_SGTTY_H)
BRLCAD_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
BRLCAD_INCLUDE_FILE(string.h HAVE_STRING_H)
BRLCAD_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
BRLCAD_INCLUDE_FILE(sys/_ioctl.h HAVE_SYS__IOCTL_H)
BRLCAD_INCLUDE_FILE(sys/file.h HAVE_SYS_FILE_H)
BRLCAD_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H)
BRLCAD_INCLUDE_FILE(sys/ioctl_compat.h HAVE_SYS_IOCTL_COMPAT_H)
BRLCAD_INCLUDE_FILE(sys/machd.h HAVE_SYS_MACHD_H)
BRLCAD_INCLUDE_FILE(sys/mman.h HAVE_SYS_MMAN_H)
BRLCAD_INCLUDE_FILE(sys/mount.h HAVE_SYS_MOUNT_H)
BRLCAD_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)
BRLCAD_INCLUDE_FILE(sys/prctl.h HAVE_SYS_PRCTL_H)
BRLCAD_INCLUDE_FILE(sys/sched.h HAVE_SYS_SCHED_H)
BRLCAD_INCLUDE_FILE(sys/select.h HAVE_SYS_SELECT_H)
BRLCAD_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
BRLCAD_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
BRLCAD_INCLUDE_FILE(sys/sysctl.h HAVE_SYS_SYSCTL_H)
BRLCAD_INCLUDE_FILE(sys/sysinfo.h HAVE_SYS_SYSINFO_H)
BRLCAD_INCLUDE_FILE(sys/sysmp.h HAVE_SYS_SYSMP_H)
BRLCAD_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
BRLCAD_INCLUDE_FILE(sys/times.h HAVE_SYS_TIMES_H)
BRLCAD_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
BRLCAD_INCLUDE_FILE(sys/un.h HAVE_SYS_UN_H)
BRLCAD_INCLUDE_FILE(sys/wait.h HAVE_SYS_WAIT_H)
BRLCAD_INCLUDE_FILE(syslog.h HAVE_SYSLOG_H)
BRLCAD_INCLUDE_FILE(term.h HAVE_TERM_H)
BRLCAD_INCLUDE_FILE(termcap.h HAVE_TERMCAP_H)
BRLCAD_INCLUDE_FILE(terminfo.h HAVE_TERMINFO_H)
BRLCAD_INCLUDE_FILE(termio.h HAVE_TERMIO_H)
BRLCAD_INCLUDE_FILE(termios.h HAVE_TERMIOS_H)
BRLCAD_INCLUDE_FILE(termlib.h HAVE_TERMLIB_H)
BRLCAD_INCLUDE_FILE(tinfo.h HAVE_TINFO_H)
BRLCAD_INCLUDE_FILE(ulocks.h HAVE_ULOCKS_H)
BRLCAD_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
BRLCAD_INCLUDE_FILE(windows.h HAVE_WINDOWS_H) # for QueryPerformanceCounter() on Windows
BRLCAD_INCLUDE_FILE(winsock.h HAVE_WINSOCK_H)

# C++
BRLCAD_INCLUDE_FILE_CXX(istream HAVE_ISTREAM)
BRLCAD_INCLUDE_FILE_CXX(limits HAVE_LIMITS)

# Other special-case tests that need custom macros
BRLCAD_CHECK_BASENAME()
BRLCAD_CHECK_DIRNAME()


# *******************************************************************
# ***         Check for Types/Structures - Stage 6 of 9           ***
# *******************************************************************

BRLCAD_STRUCT_MEMBER("struct stat" st_blksize sys/stat.h STRUCT_STAT_ST_BLKSIZE)
BRLCAD_STRUCT_MEMBER("struct stat" st_blocks sys/stat.h STRUCT_STAT_ST_BLOCKS)
BRLCAD_STRUCT_MEMBER("struct stat" st_rdev sys/stat.h STRUCT_STAT_ST_RDEV)

# socklen_t
BRLCAD_TYPE_SIZE("socklen_t" SOCKLEN_T "sys/types.h")
if(NOT SOCKLEN_T)
  BRLCAD_TYPE_SIZE("socklen_t" SOCKLEN_T "sys/socket.h")
endif(NOT SOCKLEN_T)

BRLCAD_TYPE_SIZE("int" INT "")
BRLCAD_TYPE_SIZE("long" LONG "")
BRLCAD_TYPE_SIZE("long long" LONG_LONG "")
BRLCAD_TYPE_SIZE("size_t" SIZE "")
BRLCAD_TYPE_SIZE("ssize_t" SSIZE "")
BRLCAD_TYPE_SIZE("uint64_t" UINT64 "")
BRLCAD_TYPE_SIZE("uintptr_t" UINTPTR "")
BRLCAD_TYPE_SIZE("sig_t" SIG "signal.h")

# see if the compiler supports %z as a size_t print width specifier
BRLCAD_CHECK_C99_FORMAT_SPECIFIERS()


# *******************************************************************
# ***             Check for Functions - Stage 7 of 9              ***
# *******************************************************************

# ALLOCA test - based on AC_FUNC_ALLOCA
BRLCAD_ALLOCA()

BRLCAD_FUNCTION_EXISTS(XQueryExtension HAVE_XQUERYEXTENSION)
BRLCAD_FUNCTION_EXISTS(alarm HAVE_ALARM)
BRLCAD_FUNCTION_EXISTS(dlopen HAVE_DLOPEN)
BRLCAD_FUNCTION_EXISTS(drand48 HAVE_DRAND48)
BRLCAD_FUNCTION_EXISTS(fchmod HAVE_FCHMOD)
BRLCAD_FUNCTION_EXISTS(fsync HAVE_FSYNC)
BRLCAD_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
BRLCAD_FUNCTION_EXISTS(getegid HAVE_GETEGID)
BRLCAD_FUNCTION_EXISTS(geteuid HAVE_GETEUID)
BRLCAD_FUNCTION_EXISTS(gethostname HAVE_GETHOSTNAME)
BRLCAD_FUNCTION_EXISTS(getloadavg HAVE_GETLOADAVG)
BRLCAD_FUNCTION_EXISTS(getopt_long HAVE_GETOPT_LONG)
BRLCAD_FUNCTION_EXISTS(getprogname HAVE_GETPROGNAME)
BRLCAD_FUNCTION_EXISTS(kill HAVE_KILL)
BRLCAD_FUNCTION_EXISTS(lrand48 HAVE_LRAND48)
BRLCAD_FUNCTION_EXISTS(memset HAVE_MEMSET)
BRLCAD_FUNCTION_EXISTS(mkstemp HAVE_MKSTEMP)
BRLCAD_FUNCTION_EXISTS(pipe HAVE_PIPE)
BRLCAD_FUNCTION_EXISTS(popen HAVE_POPEN)
BRLCAD_FUNCTION_EXISTS(random HAVE_RANDOM)
BRLCAD_FUNCTION_EXISTS(realpath HAVE_REALPATH)
BRLCAD_FUNCTION_EXISTS(sbrk HAVE_SBRK)
BRLCAD_FUNCTION_EXISTS(setenv HAVE_SETENV)
BRLCAD_FUNCTION_EXISTS(setpgid HAVE_SETPGID)
BRLCAD_FUNCTION_EXISTS(setpriority HAVE_SETPRIORITY)
BRLCAD_FUNCTION_EXISTS(setprogname HAVE_SETPROGNAME)
BRLCAD_FUNCTION_EXISTS(srand48 HAVE_SRAND48)
BRLCAD_FUNCTION_EXISTS(strchr HAVE_STRCHR)
BRLCAD_FUNCTION_EXISTS(strdup HAVE_STRDUP)
BRLCAD_FUNCTION_EXISTS(strlcat HAVE_STRLCAT)
BRLCAD_FUNCTION_EXISTS(strlcpy HAVE_STRLCPY)
BRLCAD_FUNCTION_EXISTS(strsep HAVE_STRSEP)
BRLCAD_FUNCTION_EXISTS(strtok HAVE_STRTOK)
BRLCAD_FUNCTION_EXISTS(sync HAVE_SYNC)
BRLCAD_FUNCTION_EXISTS(sysctl HAVE_SYSCTL)
BRLCAD_FUNCTION_EXISTS(sysmp HAVE_SYSMP)
BRLCAD_FUNCTION_EXISTS(vfork HAVE_VFORK)
BRLCAD_FUNCTION_EXISTS(vsscanf HAVE_VSSCANF)
BRLCAD_FUNCTION_EXISTS(writev HAVE_WRITEV)

# On Windows, we need to check for hypot.  This test pertains
# to the windows specific config file, not CONFIG_H_FILE - hence,
# just run the test and it will be handled by configure_file later.
include(CheckSymbolExists)
if(WIN32)
  # consider all warnings as errors (MSVC)
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} /WX)
  CHECK_SYMBOL_EXISTS(hypot "math.h" HAVE_HYPOT)
  #If we don't have it, need to define it as _hypot in config_win
  if(NOT HAVE_HYPOT)
    set(hypot 1)
  endif(NOT HAVE_HYPOT)
endif(WIN32)

# Check whether we need to add import/export lines to libraries
if(MSVC)
  set(CPP_DLL_DEFINES 1)
endif(MSVC)

# *******************************************************************
# ***          Check for System Services - Stage 8 of 9           ***
# *******************************************************************

# For Windows, we need some dlls to be redistributed with the
# installer
if(MSVC)
  # By default, do not warn when built on machines using only VS Express
  # From: http://www.cmake.org/pipermail/cmake/2011-May/044166.html
  if(NOT DEFINED CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
  endif()
  include(InstallRequiredSystemLibraries)
endif(MSVC)

# Before we finalize, set some specific global linker flags
if(MSVC)
  set(CMAKE_EXE_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
  set(CMAKE_SHARED_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
  set(CMAKE_MODULE_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
endif(MSVC)

# We've done the toplevel configure steps, now add the subdirectories
add_subdirectory(src)
add_subdirectory(db)
add_subdirectory(include)
add_subdirectory(doc)
add_subdirectory(sh)
add_subdirectory(misc)
add_subdirectory(pix)
add_subdirectory(bench)
add_subdirectory(regress)

# Now that all the tests are done, configure the brlcad_config.h file:
file(APPEND ${CONFIG_H_FILE} "#endif /* __CONFIG_H__ */\n")
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)

# Generate the windows specific file, if needed
if(WIN32)
  configure_file(${BRLCAD_SOURCE_DIR}/include/config_win_cmake.h.in	${BRLCAD_BINARY_DIR}/include/config_win_cmake.h)
endif(WIN32)

# *******************************************************************
# ***              Summarize Results - Stage 9 of 9               ***
# *******************************************************************

# Now that everything is configured, print a summary of the build settings.  This is a bit complicated, but
# the basic idea is to print labels, "smart" spacers around those labels to line everything up, and the
# value of the appropriate build variable.  The convention is to assign the text labeling each value to a
# variable named <variable_name>_LABEL - this allows a loop to iterate over a list of variables and handle
# them all, rather than each line needing its own logic.

if(NOT BRLCAD_DISABLE_SUMMARY)
  message("\n")
  message("------ BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build  ------")
  message("\n")

  # Directory settings

  set(CMAKE_INSTALL_PREFIX_LABEL "Prefix")
  set(BIN_DIR_LABEL "Binaries")
  set(MAN_DIR_LABEL "Manual pages")
  set(CONF_DIR_LABEL "Configration files")
  set(DATA_DIR_LABEL "Data resource files")
  set(PATH_LABELS CMAKE_INSTALL_PREFIX BIN_DIR MAN_DIR CONF_DIR DATA_DIR)
  set(PATH_LABEL_LENGTH 0)
  foreach(path_label ${PATH_LABELS})
    string(LENGTH "${${path_label}_LABEL}" CURRENT_LENGTH)
    if (${CURRENT_LENGTH} GREATER ${PATH_LABEL_LENGTH})
      set(PATH_LABEL_LENGTH ${CURRENT_LENGTH})
    endif()
  endforeach()
  foreach(path_label ${PATH_LABELS})
    set(CURRENT_LABEL ${${path_label}_LABEL})
    string(LENGTH ${CURRENT_LABEL} CURRENT_LENGTH)
    while(${PATH_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
      set(CURRENT_LABEL " ${CURRENT_LABEL}")
      string(LENGTH "${CURRENT_LABEL}" CURRENT_LENGTH)
    endwhile(${PATH_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
    if(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
      message("${CURRENT_LABEL}: ${${path_label}}")
    else(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
      message("${CURRENT_LABEL}: ${CMAKE_INSTALL_PREFIX}/${${path_label}}")
    endif(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
  endforeach()
  message("\n")

  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
  if(NOT BUILD_TYPE)
    set(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_ALL_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
  else(NOT BUILD_TYPE)
    set(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS_${BUILD_TYPE}}")
    set(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS_${BUILD_TYPE}}")
    set(CMAKE_ALL_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE}}")
  endif(NOT BUILD_TYPE)

  # Compiler settings - in this case there are additional "special case" flags that get
  # added depending on build type - roll all flags into individual variables before printing

  set(CMAKE_C_COMPILER_LABEL "CC")
  set(CMAKE_CXX_COMPILER_LABEL "CXX")
  set(CMAKE_ALL_C_FLAGS_LABEL "CFLAGS")
  set(CMAKE_ALL_CXX_FLAGS_LABEL "CXXFLAGS")
  set(CMAKE_ALL_SHARED_LINKER_FLAGS_LABEL "LDFLAGS")
  set(SETTINGLABELS CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_ALL_C_FLAGS	CMAKE_ALL_CXX_FLAGS CMAKE_ALL_SHARED_LINKER_FLAGS)
  set(SETTING_LABEL_LENGTH 0)
  foreach(setting_label ${SETTINGLABELS})
    string(LENGTH ${${setting_label}_LABEL} CURRENT_LENGTH)
    if (${CURRENT_LENGTH} GREATER ${SETTING_LABEL_LENGTH})
      set(SETTING_LABEL_LENGTH ${CURRENT_LENGTH})
    endif()
  endforeach()
  foreach(setting_label ${SETTINGLABELS})
    set(CURRENT_LABEL ${${setting_label}_LABEL})
    string(LENGTH ${CURRENT_LABEL} CURRENT_LENGTH)
    while(${SETTING_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
      set(CURRENT_LABEL "${CURRENT_LABEL} ")
      string(LENGTH ${CURRENT_LABEL} CURRENT_LENGTH)
    endwhile(${SETTING_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
    message("${CURRENT_LABEL} = ${${setting_label}}")
  endforeach()
  message("\n")

  # The actual build state (as opposed to the AUTO/BUNDLED/SYSTEM setting) is
  # not present in the global cache and must be explicitly pulled from
  # src/other
  MACRO(GET_STATE_LABEL ITEM)
    get_directory_property(BRLCAD_BUILD_${ITEM} DIRECTORY src/other DEFINITION BRLCAD_${ITEM}_BUILD)
    get_directory_property(BRLCAD_${ITEM}_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_${ITEM}_NOTFOUND)
    if("${BRLCAD_BUILD_${ITEM}}" STREQUAL "OFF" AND BRLCAD_${ITEM}_NOTFOUND)
      set(BRLCAD_BUILD_${ITEM} "OFF!")
    endif("${BRLCAD_BUILD_${ITEM}}" STREQUAL "OFF" AND BRLCAD_${ITEM}_NOTFOUND)
  ENDMACRO(GET_STATE_LABEL ${ITEM})
  set(OTHER_ITEM_LIST TCL TK IWIDGETS TKHTML TKPNG TKTABLE PNG REGEX ZLIB TERMLIB UTAHRLE OPENNURBS SCL)
  foreach(item ${OTHER_ITEM_LIST})
    GET_STATE_LABEL(${item})
  endforeach(item ${OTHER_ITEM_LIST})
  get_directory_property(BRLCAD_BUILD_ITCL DIRECTORY src/other DEFINITION	BRLCAD_ITCL_BUILD)
  get_directory_property(BRLCAD_BUILD_ITK DIRECTORY src/other DEFINITION BRLCAD_ITK_BUILD)
  get_directory_property(BRLCAD_ITCL_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_ITCL_NOTFOUND)
  get_directory_property(BRLCAD_ITK_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_ITK_NOTFOUND)
  if(BRLCAD_BUILD_ITCL AND BRLCAD_BUILD_ITK)
    set(BRLCAD_BUILD_INCRTCL ON)
  else(BRLCAD_BUILD_ITCL AND BRLCAD_BUILD_ITK)
    if(BRLCAD_BUILD_ITCL AND NOT BRLCAD_BUILD_ITK)
      set(BRLCAD_BUILD_INCRTCL "ON (Itcl only)")
    endif(BRLCAD_BUILD_ITCL AND NOT BRLCAD_BUILD_ITK)
    if(BRLCAD_BUILD_ITK AND NOT BRLCAD_BUILD_ITCL)
      set(BRLCAD_BUILD_INCRTCL "ON (Itk only)")
    endif(BRLCAD_BUILD_ITK AND NOT BRLCAD_BUILD_ITCL)
    if(NOT BRLCAD_BUILD_ITCL AND NOT BRLCAD_BUILD_ITK)
      if(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
	set(BRLCAD_BUILD_INCRTCL "OFF!")
      else(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
	set(BRLCAD_BUILD_INCRTCL "OFF")
      endif(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
    endif(NOT BRLCAD_BUILD_ITCL AND NOT BRLCAD_BUILD_ITK)
  endif(BRLCAD_BUILD_ITCL AND BRLCAD_BUILD_ITK)

  # Build options
  set(BRLCAD_BUILD_TCL_LABEL "Compile Tcl ")
  set(BRLCAD_BUILD_TK_LABEL "Compile Tk ")
  set(BRLCAD_BUILD_INCRTCL_LABEL "Compile Itcl/Itk ")
  set(BRLCAD_BUILD_IWIDGETS_LABEL "Compile Iwidgets ")
  set(BRLCAD_BUILD_TKHTML_LABEL "Compile Tkhtml ")
  set(BRLCAD_BUILD_TKPNG_LABEL "Compile tkpng ")
  set(BRLCAD_BUILD_TKTABLE_LABEL "Compile Tktable ")
  set(BRLCAD_BUILD_PNG_LABEL "Compile libpng ")
  set(BRLCAD_BUILD_REGEX_LABEL "Compile libregex ")
  set(BRLCAD_BUILD_ZLIB_LABEL "Compile zlib ")
  set(BRLCAD_BUILD_TERMLIB_LABEL "Compile termlib ")
  set(BRLCAD_BUILD_UTAHRLE_LABEL "Compile Utah Raster Toolkit ")
  set(BRLCAD_BUILD_OPENNURBS_LABEL "Compile openNURBS ")
  set(BRLCAD_BUILD_SCL_LABEL "Compile NIST STEP Class Libraries ")
  set(BRLCAD_ENABLE_X11_LABEL "X11 support (optional) ")
  set(BRLCAD_ENABLE_OPENGL_LABEL "OpenGL support (optional) ")
  set(BRLCAD_ENABLE_RTSERVER_LABEL "librtserver JDK support (optional) ")
  set(BRLCAD_ENABLE_RUNTIME_DEBUG_LABEL "Enable run-time debugging (optional) ")
  if(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
    set(BRLCAD_ARCH_BITSETTING "${CMAKE_WORD_SIZE} (Auto)")
  else(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
    set(BRLCAD_ARCH_BITSETTING "${CMAKE_WORD_SIZE}")
  endif(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  set(BRLCAD_ARCH_BITSETTING_LABEL "Build 32/64-bit release ")
  set(BRLCAD_OPTIMIZED_BUILD_LABEL "Build optimized release ")
  set(BRLCAD_DEBUG_BUILD_LABEL "Build debuggable release")
  set(BRLCAD_ENABLE_PROFILING_LABEL "Build profile release ")
  set(BRLCAD_ENABLE_SMP_LABEL "Build SMP-capable release ")
  set(BUILD_STATIC_LIBS_LABEL "Build static libraries ")
  set(BUILD_SHARED_LIBS_LABEL "Build dynamic libraries ")
  set(BRLCAD_ENABLE_COMPILER_WARNINGS_LABEL "Print verbose compilation warnings ")
  set(BRLCAD_ENABLE_VERBOSE_PROGRESS_LABEL "Print verbose compilation progress ")
  set(BRLCAD_INSTALL_EXAMPLE_GEOMETRY_LABEL "Install example geometry models ")
  if (BRLCAD_EXTRADOCS)
    if (BRLCAD_EXTRADOCS_PDF)
      set(BRLCAD_BUILD_DOCBUILD_MSG "ON (man/html/pdf)")
    else(BRLCAD_EXTRADOCS_PDF)
      set(BRLCAD_BUILD_DOCBUILD_MSG "ON (man/html only)")
    endif(BRLCAD_EXTRADOCS_PDF)
  else (BRLCAD_BUILD_EXTRADOCS)
    set(BRLCAD_BUILD_DOCBUILD_MSG "OFF")
  endif (BRLCAD_EXTRADOCS)
  set(BRLCAD_BUILD_DOCBUILD_MSG_LABEL "Generate extra docs ")

  set(SETTINGLIST1 BRLCAD_BUILD_TCL BRLCAD_BUILD_TK BRLCAD_BUILD_INCRTCL BRLCAD_BUILD_IWIDGETS BRLCAD_BUILD_TKHTML BRLCAD_BUILD_TKPNG BRLCAD_BUILD_TKTABLE BRLCAD_BUILD_PNG BRLCAD_BUILD_REGEX BRLCAD_BUILD_ZLIB BRLCAD_BUILD_TERMLIB BRLCAD_BUILD_UTAHRLE BRLCAD_BUILD_OPENNURBS BRLCAD_BUILD_SCL)
  set(SETTINGLIST2 BRLCAD_ENABLE_X11 BRLCAD_ENABLE_OPENGL BRLCAD_ENABLE_RTSERVER BRLCAD_ENABLE_RUNTIME_DEBUG )
  set(SETTINGLIST3 BRLCAD_ARCH_BITSETTING BRLCAD_OPTIMIZED_BUILD
    BRLCAD_DEBUG_BUILD BRLCAD_ENABLE_PROFILING BRLCAD_ENABLE_SMP BUILD_STATIC_LIBS BUILD_SHARED_LIBS BRLCAD_ENABLE_COMPILER_WARNINGS BRLCAD_ENABLE_VERBOSE_PROGRESS BRLCAD_INSTALL_EXAMPLE_GEOMETRY BRLCAD_BUILD_DOCBUILD_MSG)
  set(ARRAYLIST SETTINGLIST1 SETTINGLIST2 SETTINGLIST3)
  set(SETTING_LABEL_LENGTH 0)
  foreach(setting_array ${ARRAYLIST})
    foreach(setting_label ${${setting_array}})
      string(LENGTH ${${setting_label}_LABEL} CURRENT_LENGTH)
      if (${CURRENT_LENGTH} GREATER ${SETTING_LABEL_LENGTH})
	set(SETTING_LABEL_LENGTH ${CURRENT_LENGTH})
      endif()
    endforeach()
  endforeach()
  foreach(setting_array ${ARRAYLIST})
    foreach(setting_label ${${setting_array}})
      set(CURRENT_LABEL ${${setting_label}_LABEL})
      string(LENGTH ${CURRENT_LABEL} CURRENT_LENGTH)
      while(${SETTING_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
	set(CURRENT_LABEL "${CURRENT_LABEL}.")
	string(LENGTH ${CURRENT_LABEL} CURRENT_LENGTH)
      endwhile(${SETTING_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
      message("${CURRENT_LABEL}..: ${${setting_label}}")
    endforeach()
    message("\n")
  endforeach()

endif(NOT BRLCAD_DISABLE_SUMMARY)

if(NOT IS_SUBBUILD)
  # Set up rules to print a timestamp string during build
  set(BUILD_DELTA_FILE "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_FILE")
  set(BUILD_DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_START")
  set(BUILD_DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_END")
  configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/builddelta_start.c.in
    ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c)
  configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/builddelta_end.c.in
    ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c)
  add_executable(printtimestamp ${BRLCAD_CMAKE_DIR}/test_srcs/print_timestamp.c)
  add_executable(buildtimestart ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c)
  add_executable(buildtimeend ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c)
  add_custom_command(
    OUTPUT ${BUILD_DELTA_FILE}
    COMMAND buildtimestart
    COMMENT ""
    DEPENDS buildtimestart
    )
  add_custom_target(timestamp ALL
    COMMAND printtimestamp
    COMMAND ${CMAKE_COMMAND} -E rename ${BUILD_DELTA_FILE} ${BUILD_DELTA_START}
    DEPENDS ${BUILD_DELTA_FILE} printtimestamp
    )
  add_custom_target(buildtimedelta ALL
    COMMAND buildtimeend
    COMMAND ${CMAKE_COMMAND} -E remove ${BUILD_DELTA_START}
    DEPENDS buildtimeend
    )
endif(NOT IS_SUBBUILD)

# We want the timestamp to come first, so make ALL library,
# exec and external targets depend on timestamp (except for
# the exec target printtimestamp, which is needed by timestamp)
# Similarly, buildtimedelta needs to depend on every target.
if(NOT IS_SUBBUILD)
  get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
  get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
  get_property(CMAKE_CUSTOM_TARGET_LIST GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST)
  mark_as_advanced(CMAKE_LIBRARY_TARGET_LIST)
  mark_as_advanced(CMAKE_EXEC_TARGET_LIST)
  mark_as_advanced(CMAKE_CUSTOM_TARGET_LIST)
  LIST(REMOVE_DUPLICATES CMAKE_LIBRARY_TARGET_LIST)
  LIST(REMOVE_DUPLICATES CMAKE_EXEC_TARGET_LIST)
  LIST(REMOVE_DUPLICATES CMAKE_CUSTOM_TARGET_LIST)
  if(CMAKE_EXTERNAL_TARGET_LIST)
    LIST(REMOVE_DUPLICATES CMAKE_EXTERNAL_TARGET_LIST)
  endif(CMAKE_EXTERNAL_TARGET_LIST)
  foreach(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
    get_target_property(target_location ${libtarget} LOCATION_${CMAKE_BUILD_TYPE})
    if(target_location)
      ADD_DEPENDENCIES(${libtarget} timestamp)
      ADD_DEPENDENCIES(buildtimedelta ${libtarget})
    endif(target_location)
  endforeach(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
  foreach(exectarget ${CMAKE_EXEC_TARGET_LIST})
    get_target_property(target_location ${exectarget} LOCATION_${CMAKE_BUILD_TYPE})
    if(target_location)
      if(NOT ${exectarget} MATCHES "printtimestamp"
	  AND NOT ${custtarget} MATCHES "buildtimestart")
	ADD_DEPENDENCIES(${exectarget} timestamp)
      endif()
      ADD_DEPENDENCIES(buildtimedelta ${exectarget})
    endif(target_location)
  endforeach(exectarget ${CMAKE_EXEC_TARGET_LIST})
  foreach(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
    if(NOT ${custtarget} MATCHES "timestamp")
      ADD_DEPENDENCIES(${custtarget} timestamp)
    endif()
    if(NOT ${custtarget} MATCHES "buildtimedelta")
      get_target_property(not_in_all ${custtarget} EXCLUDE_FROM_ALL)
      if(NOT not_in_all)
	ADD_DEPENDENCIES(buildtimedelta ${custtarget})
      endif(NOT not_in_all)
    endif(NOT ${custtarget} MATCHES "buildtimedelta")
  endforeach(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
  foreach(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
    get_target_property(target_confcmd ${externaltarget} _EP_CONFIGURE_COMMAND)
    if(target_confcmd)
      ADD_DEPENDENCIES(${externaltarget} timestamp)
      ADD_DEPENDENCIES(buildtimedelta ${externaltarget})
    endif(target_confcmd)
  endforeach(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
endif(NOT IS_SUBBUILD)

# Generate the include/conf files

# HOST
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_hostname.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c)
if(NOT MSVC)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    ${CMAKE_BINARY_DIR}/CMakeTmp
    ${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c
    OUTPUT_VARIABLE COMPILEMESSAGES)
else(NOT MSVC)
  set(CMAKE_REQUIRED_LIBRARIES ws2_32.lib)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    ${CMAKE_BINARY_DIR}/CMakeTmp
    ${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c
    OUTPUT_VARIABLE COMPILEMESSAGES
    CMAKE_FLAGS "-DLINK_LIBRARIES:STRING=ws2_32.lib")
endif(NOT MSVC)
# Let CMake give us the proper newline character(s) for the current platform
file(APPEND ${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp "\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp ${BRLCAD_BINARY_DIR}/include/conf/HOST)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove	${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp)

# USER
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_username.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c)
TRY_RUN(TIME_RESULT TIME_COMPILED
  ${CMAKE_BINARY_DIR}/CMakeTmp
  ${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c
  OUTPUT_VARIABLE COMPILEMESSAGES)
# Let CMake give us the proper newline character(s) for the current platform
file(APPEND ${BRLCAD_BINARY_DIR}/include/conf/USER.tmp "\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BRLCAD_BINARY_DIR}/include/conf/USER.tmp ${BRLCAD_BINARY_DIR}/include/conf/USER)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${BRLCAD_BINARY_DIR}/include/conf/USER.tmp)

# PATH
file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/PATH.tmp "\"${CMAKE_INSTALL_PREFIX}\"\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BRLCAD_BINARY_DIR}/include/conf/PATH.tmp ${BRLCAD_BINARY_DIR}/include/conf/PATH)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove	${BRLCAD_BINARY_DIR}/include/conf/PATH.tmp)

# COUNT - Count how many times the configuration has changed.  Currently
# we are detecting a configuration change by looking at the previous version
# of the CMakeCache.txt file and looking for changes between values stored
# there and settings in the current environment.  This has the disadvantage
# of not spotting completely new variables in the current enviornment, but
# the internal CMake variable list is too noisy so at the moment this looks
# like the best alternative we have.

include(${CMAKE_SOURCE_DIR}/misc/CMake/DiffCache.cmake)
#force CMake to look every time in case of manual removal of COUNT file.
set(countFile "countFile-NOTFOUND" CACHE STRING "Is there a count file?" FORCE)
find_file(countFile COUNT PATHS ${BRLCAD_BINARY_DIR}/include/conf)
if(countFile MATCHES "^countFile-NOTFOUND$")
  set(buildCounter 0)
else()
  file(READ ${countFile} buildCounter_raw)
  string(STRIP ${buildCounter_raw} buildCounter)
  DIFF_CACHE_file()
  if(INCREMENT_COUNT_FILE)
    math(EXPR buildCounter "${buildCounter} + 1")
  endif(INCREMENT_COUNT_FILE)
endif()
file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/COUNT.tmp "${buildCounter}\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BRLCAD_BINARY_DIR}/include/conf/COUNT.tmp ${BRLCAD_BINARY_DIR}/include/conf/COUNT)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove	${BRLCAD_BINARY_DIR}/include/conf/COUNT.tmp)

# make sure the cache reports this found, to avoid spurious config difference
find_file(countFile COUNT PATHS ${BRLCAD_BINARY_DIR}/include/conf)
mark_as_advanced(countFile)

# DATE - Increment this when we increment the COUNT, to avoid triggering
# excessive relinking (an RFC2822 timestamp changes every second).
file(READ ${CONFIG_TIMESTAMP_FILE} DATESTAMP)
string(STRIP ${DATESTAMP} DATESTAMP)
if(INCREMENT_COUNT_FILE OR NOT EXISTS ${BRLCAD_BINARY_DIR}/include/conf/DATE)
  file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp "\"${DATESTAMP}\"\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp ${BRLCAD_BINARY_DIR}/include/conf/DATE)
  execute_process(COMMAND ${CMAKE_COMMAND} -E remove	${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp)
endif(INCREMENT_COUNT_FILE OR NOT EXISTS ${BRLCAD_BINARY_DIR}/include/conf/DATE)

# Rules for the toplevel documentation files
set(toplevel_DOCFILES
  AUTHORS
  COPYING
  HACKING
  INSTALL
  NEWS
  README
  )
file(COPY ${toplevel_DOCFILES} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR})
install(FILES ${toplevel_DOCFILES} DESTINATION ${DATA_DIR})

# We want to ignore an in-source build directory if possible, try to recognize
# that case and get the appropriate subpath for CPACK.  This isn't workable if
# the build directory IS the source directory - in such a case the working
# files of the build are mixed with the sources and an exclusion list is
# pretty hopeless.
if(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  set(TEST_BUILD_PATH "${CMAKE_BINARY_DIR}/")
  set(TEST_SOURCE_PATH "${CMAKE_SOURCE_DIR}/")
  string(REGEX REPLACE "${TEST_SOURCE_PATH}" "" CMAKE_BUILD_DIR_TRIMMED "${TEST_BUILD_PATH}")
  if(NOT ${TEST_BUILD_PATH} STREQUAL ${CMAKE_BUILD_DIR_TRIMMED})
    set(CPACK_IGNORE_BUILD_PATH "${CMAKE_BUILD_DIR_TRIMMED}")
  endif(NOT ${TEST_BUILD_PATH} STREQUAL ${CMAKE_BUILD_DIR_TRIMMED})
endif(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

# CPack is used to produce tgz files, RPMS, etc.  If SUBBUILD is enabled this
# becomes the responsibility of the parent project.
if(NOT IS_SUBBUILD)
  option(BRLCAD_VERSIONED_RPM_NAME "Incorporate BRL-CAD version into RPM package name" OFF)
  mark_as_advanced(BRLCAD_VERSIONED_RPM_NAME)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BRL-CAD - a powerful cross-platform open source solid modeling system")
  set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
  set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README)
  set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
  set(CPACK_PACKAGE_VERSION_MAJOR ${BRLCAD_MAJOR_VERSION})
  set(CPACK_PACKAGE_VERSION_MINOR ${BRLCAD_MINOR_VERSION})
  set(CPACK_PACKAGE_VERSION_PATCH ${BRLCAD_PATCH_VERSION})
  if(BRLCAD_VERSIONED_RPM_NAME)
    set(CPACK_RPM_PACKAGE_NAME "BRL-CAD_${BRLCAD_VERSION}")
  endif(BRLCAD_VERSIONED_RPM_NAME)

  if(UNIX)
    set(CPACK_STRIP_FILES TRUE)
  endif(UNIX)

  if(UNIX)
    set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
  endif(UNIX)

  if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR TGZ TBZ2)

    find_program(RPMBUILD_EXEC rpmbuild)
    mark_as_advanced(RPMBUILD_EXEC)
    if(RPMBUILD_EXEC)
      set(CPACK_GENERATOR ${CPACK_GENERATOR} RPM)
      set(CPACK_RPM_PACKAGE_LICENSE "LGPL 2.1")
      set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
      #set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_MAJOR_VERSION}_${BRLCAD_MINOR_VERSION}_${BRLCAD_PATCH_VERSION}")
      #if(EXISTS /etc/redhat-release)
      #	file(READ /etc/redhat-release REDHAT_RELEASE)
      #	string(REGEX MATCH "[0-9]+" REDHAT_VERSION ${REDHAT_RELEASE})
      #	string(REGEX MATCH "Enterprise Linux" LINUX_DIST_TYPE ${REDHAT_RELEASE})
      #	if(LINUX_DIST_TYPE)
      #		set(LINUX_DIST_TYPE "el")
      #	else(LINUX_DIST_TYPE)
      #		set(LINUX_DIST_TYPE "rh")
      #	endif(LINUX_DIST_TYPE)
      #	set(CPACK_RPM_PACKAGE_RELEASE 2.${LINUX_DIST_TYPE}${REDHAT_VERSION})
      #else(EXISTS /etc/redhat-release)
      #	#set(CPACK_RPM_PACKAGE_RELEASE 2)
      #endif(EXISTS /etc/redhat-release)
    endif(RPMBUILD_EXEC)
    if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_x86")
    else(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
    endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  else(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
  endif(UNIX AND NOT APPLE)

  if(WIN32)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD")
    set(CPACK_SOURCE_DIR ${CMAKE_SOURCE_DIR})
    set(CPACK_DATA_DIR "share\\\\brlcad\\\\${BRLCAD_VERSION}")
    # There is a bug in NSI that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backlasshes.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\brlcad.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\uninstall.ico")
    set(CPACK_NSIS_DISPLAY_NAME "BRL-CAD")
    set(CPACK_NSIS_MODIFY_PATH ON)
  endif(WIN32)

  set(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "brlcad-${BRLCAD_VERSION}")
  set(CPACK_SOURCE_IGNORE_FILES "\\\\.svn/" "${CPACK_IGNORE_BUILD_PATH}")

  CONFIGURE_file("${BRLCAD_CMAKE_DIR}/BRLCAD_CPackOptions.cmake.in"	"${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake" @ONLY)
  set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")

  include(CPack)
endif(NOT IS_SUBBUILD)

# Some files to ignore for distcheck.  For this case
# only, we add CMakeLists.txt (others are handled
# by add_subdirectory wrapper
set(toplevel_ignore_files
  ${toplevel_DOCFILES}
  BUGS
  ChangeLog
  TODO
  autogen.sh
  configure.ac
  configure.cmake.sh
  m4
  Makefile.am
  CMakeLists.txt
  )
CMAKEFILES(${toplevel_ignore_files})

# Define a target distcheck.  This performs the following steps to test
# whether BRL-CAD is in a distribution ready state:
#
# 1.  Check the files currently in the source directories against
#     known file lists generated by CMake and Subversion. There are
#     two hard fail cases - files in Subversion not accounted for by
#     the build logic, and build logic files not in Subversion.  Files
#     not accounted for by either are assumed to be temporary and will
#     be excluded from the archive files.
# 2.  Run CPack to generate source tarballs.
# 3.  Expand the tarball into a subdirectory, make a build directory,
#     cd into the build directory, and configure BRL-CAD for a local
#     build and install.
# 4.  Perform the build.
# 5.  Run the regression and benchmark targets in the build directory.
# 6.  Install to the local install directory.
# 7.  Clear the tgz expanded sources and build files.
# 8.  Run benchmark from the installed directory.
# 9.  Clean up.

# If we're doing a Make based build, we want the Make command.  Otherwise
# fall back on cmake --build, with the limitations imposed by that option.
# This really needs to migrate to it's own .cmake file, too complex for a
# series of COMMAND statements.
if(NOT IS_SUBBUILD)
  find_program(CPACK_EXEC cpack)
  mark_as_advanced(CPACK_EXEC)
  configure_file(${BRLCAD_CMAKE_DIR}/distcheck_buildsys.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake @ONLY)

  if("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
    add_custom_target(distcheck
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 1:  Check files in Source Repository against files specified in Build Logic"
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 2:  Create source tgz, tbz2 and zip archives from toplevel archive."
      COMMAND ${CPACK_EXEC} --config ${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 3:  Expand tgz archive and prepare to build from archive sources."
      COMMAND ${CMAKE_COMMAND} -E tar xvzf ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz
      COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
      COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -E chdir _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build	${CMAKE_COMMAND} ../${CPACK_SOURCE_PACKAGE_FILE_NAME}	-DBRLCAD_BUNDLED_LIBS="Bundled" -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 4:  Compile using source from tgz archive."
      COMMAND ${CMAKE_COMMAND} -E chdir _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build $(MAKE)
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 5:  Run build-directory regression and benchmark testing from tgz archive compilation."
      COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target regress
      COMMAND TIMEFRAME=1 ${CMAKE_COMMAND} --build	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target benchmark
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 6:  Install from tgz build directory."
      COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target install
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 7:  Clear tgz archive sources and build directory."
      COMMAND ${CMAKE_COMMAND} -E remove_directory	${CPACK_SOURCE_PACKAGE_FILE_NAME}
      COMMAND ${CMAKE_COMMAND} -E remove_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 8:  Run benchmark testing from tgz archive installed build."
      COMMAND TIMEFRAME=1 _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/${BIN_DIR}/benchmark run
      COMMAND _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/${BIN_DIR}/benchmark clean
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 9: Cleanup."
      COMMAND ${CMAKE_COMMAND} -E remove summary
      COMMAND ${CMAKE_COMMAND} -E remove run-*.log
      COMMAND ${CMAKE_COMMAND} -E remove_directory	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
      )
  else("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
    add_custom_target(distcheck
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 1:  Check files in Source Repository against files specified in Build Logic"
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 2:  Create source tgz, tbz2 and zip archives from toplevel archive."
      COMMAND ${CPACK_EXEC} --config ${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 3:  Expand tgz archive and prepare to build from archive sources."
      COMMAND ${CMAKE_COMMAND} -E tar xvzf ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz
      COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
      COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -E chdir _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build	${CMAKE_COMMAND} ../${CPACK_SOURCE_PACKAGE_FILE_NAME}	-DBRLCAD_BUNDLED_LIBS="Bundled" -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 4:  Compile using source from tgz archive."
      COMMAND ${CMAKE_COMMAND} -E build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 5:  Run build-directory regression and benchmark testing from tgz archive compilation."
      COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target regress
      COMMAND TIMEFRAME=1 ${CMAKE_COMMAND} --build	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target benchmark
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 6:  Install from tgz build directory."
      COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target install
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 7:  Clear tgz archive sources and build directory."
      COMMAND ${CMAKE_COMMAND} -E remove_directory	${CPACK_SOURCE_PACKAGE_FILE_NAME}
      COMMAND ${CMAKE_COMMAND} -E remove_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 8:  Run benchmark testing from tgz archive installed build."
      COMMAND TIMEFRAME=1 _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/${BIN_DIR}/benchmark run
      COMMAND _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/${BIN_DIR}/benchmark clean
      COMMAND ${CMAKE_COMMAND} -E echo "Stage 9: Cleanup."
      COMMAND ${CMAKE_COMMAND} -E remove summary
      COMMAND ${CMAKE_COMMAND} -E remove run-*.log
      COMMAND ${CMAKE_COMMAND} -E remove_directory	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_message
      )
  endif("${CMAKE_GENERATOR}" MATCHES "Make" AND ("${cmake_generator}" MATCHES "Make" OR NOT cmake_generator))
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove_directory	${CPACK_SOURCE_PACKAGE_FILE_NAME}
    COMMAND ${CMAKE_COMMAND} -E remove_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
    COMMAND ${CMAKE_COMMAND} -E remove_directory	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
    COMMAND ${CMAKE_COMMAND} -E remove summary
    COMMAND ${CMAKE_COMMAND} -E remove run-*.log
    COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_SOURCE_PACKAGE_FILE_NAME}.*
    )

endif(NOT IS_SUBBUILD)


# Mark various miscellaneous things as advanced that we don't want in our
# default view
mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY)
mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
mark_as_advanced(LIBRARY_OUTPUT_PATH)

# If options have been defined that we are going to document, by now they've
# been documented.  Make an updated copy of the install file and see whether
# anything has changed - we're not going to overwrite the copy in the src
# directory (principle of this build is to leave the src dir pristine after
# configuration and compilation) but check for changes and report if found.
file(READ ${BRLCAD_SOURCE_DIR}/INSTALL SRC_INSTALL_STR)
string(REGEX REPLACE "${CONFIG_OPT_STRING}.*" "" INSTALL_PREFIX	"${SRC_INSTALL_STR}")
file(WRITE ${BRLCAD_BINARY_DIR}/INSTALL.new "${INSTALL_PREFIX}")
file(READ ${BRLCAD_BINARY_DIR}/OPTIONS INSTALL_OPTS)
file(APPEND ${BRLCAD_BINARY_DIR}/INSTALL.new "${INSTALL_OPTS}")
file(APPEND ${BRLCAD_BINARY_DIR}/INSTALL.new "\n\n*** Note - Do not add or edit configuration option descriptions and alias
   lists in this file - those entries are auto-generated from information in
   the toplevel CMakeLists.txt file and src/other/CMakeLists.txt - any changes
   should be made in those files.  The CMake configuration process will
   automatically re-generate INSTALL with the new descriptions and alias 
   information.")
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  ${BRLCAD_SOURCE_DIR}/INSTALL ${BRLCAD_BINARY_DIR}/INSTALL.new
  RESULT_VARIABLE IS_DIFFERENT OUTPUT_VARIABLE DIFF_OUTPUT ERROR_VARIABLE
  DIFF_ERROR)
if(IS_DIFFERENT)
  message(WARNING "\n${BRLCAD_SOURCE_DIR}/INSTALL is out of date.  An updated version has been generated at ${BRLCAD_BINARY_DIR}/INSTALL.new\nTo clear this warning, replace ${BRLCAD_SOURCE_DIR}/INSTALL with ${BRLCAD_BINARY_DIR}/INSTALL.new\n")
endif(IS_DIFFERENT)

# Do the same thing for the configure shell script - finish it and check
# for differences.
file(READ ${BRLCAD_SOURCE_DIR}/misc/CMake/configure_suffix.sh CONFIG_SUFFIX)
file(APPEND ${CMAKE_BINARY_DIR}/configure.new "${CONFIG_SUFFIX}")
# Compare
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  ${BRLCAD_SOURCE_DIR}/configure.cmake.sh ${BRLCAD_BINARY_DIR}/configure.new
  RESULT_VARIABLE IS_CONFIG_SH_DIFFERENT OUTPUT_VARIABLE DIFF_OUTPUT ERROR_VARIABLE
  DIFF_ERROR)
if(IS_CONFIG_SH_DIFFERENT)
  message(WARNING "\n${BRLCAD_SOURCE_DIR}/configure.cmake.sh is out of date. An updated version has been generated at ${BRLCAD_BINARY_DIR}/configure.new\nTo clear this warning, replace ${BRLCAD_SOURCE_DIR}/configure.cmake.sh with ${BRLCAD_BINARY_DIR}/configure.new\n")
endif(IS_CONFIG_SH_DIFFERENT)


# Grab the old and the new versions
file(READ ${BRLCAD_SOURCE_DIR}/configure.cmake.sh CONFIG_SH_OLD)
file(READ ${BRLCAD_BINARY_DIR}/configure.new CONFIG_SH_NEW)

#Done with everything else - do the configure time delta
if(NOT IS_SUBBUILD)
  set(DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_END")
  configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/timedelta_end.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    ${CMAKE_BINARY_DIR}/CMakeTmp
    ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c
    OUTPUT_VARIABLE COMPILEMESSAGES)
  file(READ ${DELTA_END} CONFIG_TIME_MSG)
  string(STRIP ${CONFIG_TIME_MSG} CONFIG_TIME_MSG)
  set(CONFIG_TIME_MSG_LABEL "Elapsed configuration time")
  string(LENGTH ${CONFIG_TIME_MSG_LABEL} CURRENTLENGTH)
  while(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
    set(CONFIG_TIME_MSG_LABEL "${CONFIG_TIME_MSG_LABEL}.")
    string(LENGTH ${CONFIG_TIME_MSG_LABEL} CURRENTLENGTH)
  endwhile(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
  message("${CONFIG_TIME_MSG_LABEL}..: ${CONFIG_TIME_MSG}")
endif(NOT IS_SUBBUILD)
