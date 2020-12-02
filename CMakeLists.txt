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
# labeled sections below so that they are organized in stages as
# follows:
#
#   Stage 0 - information on the package and toplevel CMake settings
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
# The output summary should report key information about the final
# configuration of the build.  Comprehensive information is available
# in the CMake cache file in the build directory, so just hit the
# high points in the summary.
#
# After the main configure process is finished, a summary is printed
# and various settings and configuration files that require full
# knowledge of the main configure results are handled.
#
# *******************************************************************
# ***                 Top Level Settings                          ***
# *******************************************************************
# This file contains the top level CMakeLists.txt logic for the
# BRL-CAD software package.

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(BRLCAD)

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

# We will need to define a number of dependent options early on
include(CMakeDependentOption)

#---------------------------------------------------------------------
# Define an option to use OBJECT libraries - when we build this
# way, we also need position independent code.
cmake_dependent_option(USE_OBJECT_LIBS "Use OBJECT libraries" ON "NOT MSVC" OFF)
mark_as_advanced(USE_OBJECT_LIBS)
if(USE_OBJECT_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif(USE_OBJECT_LIBS)

#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.  If we
# are a subbuild, let the parent's CMAKE_MODULE_PATH supply files before
# our own, otherwise misc/CMake takes first priority.
set(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
list(APPEND CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR}")


#---------------------------------------------------------------------
# Load general utility routines for BRL-CAD CMake logic
include(BRLCAD_Util)

# Find the executable extension, if there is one
get_filename_component(EXE_EXT "${CMAKE_COMMAND}" EXT)

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
# Allow the BRLCAD_ROOT environment variable to set during build, but be noisy
# about it unless we're specifically told this is intentional.  Having this
# happen by accident is generally not a good idea.
find_program(SLEEP_EXEC sleep)
mark_as_advanced(SLEEP_EXEC)
if(NOT "$ENV{BRLCAD_ROOT}" STREQUAL "" AND NOT BRLCAD_ROOT_OVERRIDE)
  message(WARNING "\nBRLCAD_ROOT is presently set to \"$ENV{BRLCAD_ROOT}\"\nBRLCAD_ROOT should typically be used only when needed as a runtime override, not during compilation.  Building with BRLCAD_ROOT set may produce unexpected behavior during both compilation and subsequent program execution.  It is *highly* recommended that BRLCAD_ROOT be unset and not used.\n")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 2)
  endif(SLEEP_EXEC)
endif(NOT "$ENV{BRLCAD_ROOT}" STREQUAL "" AND NOT BRLCAD_ROOT_OVERRIDE)

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
# Searching the system for packages presents something of a dilemma -
# in most situations it is Very Bad for a BRL-CAD build to be using
# older versions of libraries in install directories as search results.
# Generally, the desired behavior is to ignore whatever libraries are
# in the install directories, and only use external library results if
# they are something already found on the system due to non-BRL-CAD
# installation (source compile, package managers, etc.).  Unfortunately,
# CMake's standard behavior is to add CMAKE_INSTALL_PREFIX to the search
# path once defined, resulting in (for us) the unexpected behavior of
# returning old installed libraries when CMake is re-run in a directory.
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
  if(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}" ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH "${PATH_NORMALIZED}")
  endif(NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
endif(CMAKE_INSTALL_PREFIX)

if(BRLCAD_PREFIX)
  if(NOT "${BRLCAD_PREFIX}" STREQUAL "/usr" AND NOT "${BRLCAD_PREFIX}" STREQUAL "/usr/local")
    get_filename_component(PATH_NORMALIZED "${BRLCAD_PREFIX}/${LIB_DIR}" ABSOLUTE)
    set(CMAKE_SYSTEM_IGNORE_PATH "${PATH_NORMALIZED}")
  endif(NOT "${BRLCAD_PREFIX}" STREQUAL "/usr" AND NOT "${BRLCAD_PREFIX}" STREQUAL "/usr/local")
endif(BRLCAD_PREFIX)
mark_as_advanced(CMAKE_SYSTEM_IGNORE_PATH)

#------------------------------------------------------------------------------
# For multi-configuration builds, we frequently need to know the run-time build
# directory.  Confusingly, we need to do different things for install commands
# and custom command definitions.  To manage this, we define
# CMAKE_CURRENT_BUILD_DIR_SCRIPT and CMAKE_CURRENT_BUILD_DIR_INSTALL once at
# the top level, then use them when we need the configuration-dependent path.
#
# Note that neither of these will work when we need to generate a .cmake file
# that does runtime detection of the current build configuration.  CMake
# scripts run using "cmake -P script.cmake" style invocation are independent of
# the "main" build system and will not know how to resolve either of the below
# variables correctly.  In that case, the script itself must check the current
# file in CMakeTmp/CURRENT_PATH (TODO - need to better organize and document
# this mechanism, given how critical it is proving... possible convention will
# be to have the string CURRENT_BUILD_DIR in any path that needs the relevant
# logic in a script, and a standard substitution routine...) and set any
# necessary path variables accordingly.  (TODO - make a standard function to do
# that the right way that scripts can load and use - right now that logic is
# scattered all over the code and if we wanted to change where those files went
# it would be a lot of work.)
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CURRENT_BUILD_DIR_SCRIPT "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
  set(CMAKE_CURRENT_BUILD_DIR_INSTALL "${CMAKE_BINARY_DIR}/\${BUILD_TYPE}")
else(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CURRENT_BUILD_DIR_SCRIPT "${CMAKE_BINARY_DIR}")
  set(CMAKE_CURRENT_BUILD_DIR_INSTALL "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

# Mark CMAKE_CONFIGURATION_TYPES as advanced - users shouldn't be adjusting this
# directly.
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)

#---------------------------------------------------------------------
# Set up include paths for generated header files.  For multi-config
# builds, make sure we get build-specific dirs.
if(CMAKE_CONFIGURATION_TYPES)
  include_directories(${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${INCLUDE_DIR})
  include_directories(${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${INCLUDE_DIR}/brlcad)
else(CMAKE_CONFIGURATION_TYPES)
  include_directories(${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/brlcad)
endif(CMAKE_CONFIGURATION_TYPES)

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
  set(DISTCLEAN_OUTFILES ${DISTCLEAN_OUTFILES} build.ninja rules.ninja)
endif("${CMAKE_GENERATOR}" MATCHES "Ninja")

#---------------------------------------------------------------------
# "make check" runs all of the tests (unit, benchmark, and regression) that are expected to work.
# "make unit"  runs all the unit tests.
# To build the required targets for testing in the style of GNU Autotools "make
# check") we define "unit" and "check" targets per
# http://www.cmake.org/Wiki/CMakeEmulateMakeCheck and have add_test
# automatically assemble its targets into the unit dependency list.

include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
  math(EXPR NC "${N} / 2")
  if(${NC} GREATER 1)
    set(JFLAG "-j${NC}")
  else(${NC} GREATER 1)
    set(JFLAG)
  endif(${NC} GREATER 1)
else(NOT N EQUAL 0)
  # Huh?  No j flag if we can't get a processor count
  set(JFLAG)
endif(NOT N EQUAL 0)

if(CMAKE_CONFIGURATION_TYPES)
  set(CONFIG $<CONFIG>)
else(CMAKE_CONFIGURATION_TYPES)
  if ("${CONFIG}" STREQUAL "")
    set(CONFIG "\"\"")
  endif ("${CONFIG}" STREQUAL "")
endif(CMAKE_CONFIGURATION_TYPES)

if (NOT TARGET check)
  add_custom_target(check
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"check\\\" a.k.a. \\\"BRL-CAD Validation Testing\\\" target runs"
    COMMAND ${CMAKE_COMMAND} -E echo "      BRL-CAD\\'s unit, system, integration, benchmark \\(performance\\), and"
    COMMAND ${CMAKE_COMMAND} -E echo "      regression tests.  To consider a build viable for production use,"
    COMMAND ${CMAKE_COMMAND} -E echo "      these tests must pass.  Dependencies are compiled automatically."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|STAND_ALONE\" -E \"^regress-|NOTE|benchmark|slow-\" --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -R \"benchmark\" --output-on-failure ${JFLAG}
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -L \"Regression\" --output-on-failure ${JFLAG}
    )
  set_target_properties(check PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif (NOT TARGET check)

if (NOT TARGET unit)
  add_custom_target(unit
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_COMMAND} -E echo "NOTE: The \\\"unit\\\" a.k.a. \\\"BRL-CAD Unit Testing\\\" target runs the"
    COMMAND ${CMAKE_COMMAND} -E echo "      subset of BRL-CAD\\'s API unit tests that are expected to work \\(i.e.,"
    COMMAND ${CMAKE_COMMAND} -E echo "      tests not currently under development\\).  All dependencies required"
    COMMAND ${CMAKE_COMMAND} -E echo "      by the tests are compiled automatically."
    COMMAND ${CMAKE_COMMAND} -E echo "\"**********************************************************************\""
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CONFIG} -LE \"Regression|NOT_WORKING\" -E \"^regress-|NOTE|benchmark|slow-\" ${JFLAG}
    )
  set_target_properties(unit PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif (NOT TARGET unit)

# we wrap the CMake add_test() function in order to automatically set up test
# dependencies for the 'unit' and 'check' test targets.
#
# this function extravagantly tries to work around a bug in CMake where we
# cannot pass an empty string through this wrapper to add_test().  passed as a
# list (e.g., via ARGN, ARGV, or manually composed), the empty string is
# skipped(!).  passed as a string, it is all treated as command name with no
# arguments.
#
# manual workaround used here involves invoking _add_test() with all args
# individually recreated/specified (i.e., not as a list) as this preserves
# empty strings.  this approach means we cannot generalize and only support a
# limited variety of empty string arguments, but we do test and halt if someone
# unknowingly tries.
function(BRLCAD_ADD_TEST NAME test_name COMMAND test_prog)

  # TODO - once we can require CMake 3.18, replace the empty string workaround
  # below with this cmake_language based version.  See
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21414

  # cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "")
  # foreach(_av IN LISTS ARG_UNPARSED_ARGUMENTS)
  #   string(APPEND test_args " [==[${_av}]==]")
  # endforeach()
  # cmake_language(EVAL CODE "add_test(NAME ${test_name} COMMAND ${test_args})")


  # find any occurrences of empty strings
  set(idx 0)
  set(matches)
  foreach (ARG IN LISTS ARGV)
    # need 'x' to avoid older cmake seeing "COMMAND" "STREQUAL" ""
    if ("x${ARG}" STREQUAL "x")
      list(APPEND matches ${idx})
    endif ("x${ARG}" STREQUAL "x")
    math(EXPR idx "${idx} + 1")
  endforeach()

  # make sure we don't exceed current support
  list(LENGTH matches cnt)
  if ("${cnt}" GREATER 1)
    message(FATAL_ERROR "ERROR: encountered ${cnt} > 1 empty string being passed to add_test(${test_name}).  Expand support in the top-level CMakeLists.txt file (grep add_test) or pass fewer empty strings.")
  endif ("${cnt}" GREATER 1)

  # if there are empty strings, we need to manually recreate their calling
  if ("${cnt}" GREATER 0)

    list(GET matches 0 empty)
    if ("${empty}" EQUAL 4)
      foreach (i 1)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} "" ${ARGN})
    elseif ("${empty}" EQUAL 5)
      foreach (i 1 2)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} "" ${ARGN})
    elseif ("${empty}" EQUAL 6)
      foreach (i 1 2 3)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} "" ${ARGN})
    elseif ("${empty}" EQUAL 7)
      foreach (i 1 2 3 4)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} "" ${ARGN})
    elseif ("${empty}" EQUAL 8)
      foreach (i 1 2 3 4 5)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} "" ${ARGN})
    elseif ("${empty}" EQUAL 9)
      foreach (i 1 2 3 4 5 6)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} "" ${ARGN})
    elseif ("${empty}" EQUAL 10)
      foreach (i 1 2 3 4 5 6 7)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} "" ${ARGN})
    elseif ("${empty}" EQUAL 11)
      foreach (i 1 2 3 4 5 6 7 8)
	if (ARGN)
	  list(REMOVE_AT ARGN 0)
	endif (ARGN)
      endforeach ()
      add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGV4} ${ARGV5} ${ARGV6} ${ARGV7} ${ARGV8} ${ARGV9} ${ARGV10} "" ${ARGN})


      # ADD_EMPTY_HERE: insert support for additional argv positions
      # as extra elseif tests here using the preceding pattern.  be
      # sure to update the index in the following else clause fatal
      # error message too.

    else ("${empty}" EQUAL 4)
      message(FATAL_ERROR "ERROR: encountered an empty string passed to add_test(${test_name}) as ARGV${empty} > ARGV11.  Expand support in the top-level CMakeLists.txt file (grep ADD_EMPTY_HERE).")
    endif ("${empty}" EQUAL 4)

  else ("${cnt}" GREATER 0)
    # no empty strings, no worries
    add_test(NAME ${test_name} COMMAND ${test_prog} ${ARGN})
  endif ("${cnt}" GREATER 0)


  # There are a variety of criteria that disqualify test_prog as a
  # dependency - check and return if we hit any of them.
  if (NOT TARGET ${test_prog})
    return()
  endif ()
  if ("${test_name}" MATCHES ^regress-)
    return()
  endif ()
  if ("${test_prog}" MATCHES ^regress-)
    return()
  endif ()
  if ("${test_name}" MATCHES ^slow-)
    return()
  endif ()
  if ("${test_name}" STREQUAL "benchmark")
    return()
  endif ()
  if ("${test_name}" MATCHES ^NOTE:)
    return()
  endif ()

  # add program needed for test to unit and check target dependencies
  add_dependencies(unit ${test_prog})
  add_dependencies(check ${test_prog})

endfunction(BRLCAD_ADD_TEST NAME test_name COMMAND test_prog)



#---------------------------------------------------------------------
# Load macros that will be used to define the BRL-CAD
# build logic
include(BRLCAD_Options)
include(BRLCAD_Targets)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

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
# Package creation with CMake depends on the value of umask - if permissions
# are such that temporary files are created without permissions needed for
# generated packages, the resulting packages may behave badly when installed.
# In particular, RPM packages may improperly reset permissions on core
# directories such as /usr.
function(check_umask umask_val status_var)
  string(REGEX REPLACE "[^x]" "" umask_x "${umask_val}")
  string(REGEX REPLACE "[^r]" "" umask_r "${umask_val}")
  string(LENGTH "${umask_r}" UMASK_HAVE_R)
  set(${status_var} 0 PARENT_SCOPE)
  if(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
    set(${status_var} 1 PARENT_SCOPE)
  endif(UMASK_HAVE_R AND "${umask_x}" STREQUAL "xxx")
endfunction(check_umask)

# Note - umask is not always an executable, so find_program wont' necessarily
# determine whether the umask check is appropriate.  If we don't find an
# executable, follow up to see if we can use sh to get the info.
find_program(UMASK_EXEC umask)
mark_as_advanced(UMASK_EXEC)
if(NOT UMASK_EXEC)
  # If we don't have a umask cmd, see if sh -c "umask -S" works
  execute_process(COMMAND sh -c "umask -S" OUTPUT_VARIABLE umask_out)
  # Check if we've got something that looks like a umask output
  if("${umask_out}" MATCHES "^u=.*g=.*o=.*")
    set(UMASK_EXEC sh)
    set(UMASK_EXEC_ARGS -c "umask -S")
  endif("${umask_out}" MATCHES "^u=.*g=.*o=.*")
else(NOT UMASK_EXEC)
  set(UMASK_EXEC_ARGS -S)
endif(NOT UMASK_EXEC)

if(UMASK_EXEC)
  execute_process(COMMAND ${UMASK_EXEC} ${UMASK_EXEC_ARGS} OUTPUT_VARIABLE umask_curr)
  string(STRIP "${umask_curr}" umask_curr)
  check_umask("${umask_curr}" UMASK_OK)
  if(NOT UMASK_OK)
    message(" ")
    message(WARNING "umask is set to ${umask_curr} - this setting is not recommended if one of the goals of this build is to generate packages. Use 'umask 022' for improved package behavior.")
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 1)
    endif(SLEEP_EXEC)
  endif(NOT UMASK_OK)
endif(UMASK_EXEC)

#---------------------------------------------------------------------
# print out the title with a pretty box computed to wrap around
BOX_PRINT("*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***" "*")

#---------------------------------------------------------------------
# CMake by default provides four different configurations for multi-
# configuration build tools.  We want only two - Debug and Release.
if(NOT ENABLE_ALL_CONFIG_TYPES)
  if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
  endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
endif(NOT ENABLE_ALL_CONFIG_TYPES)

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it if it exists
if (IS_DIRECTORY /usr/local)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
  if (IS_DIRECTORY /usr/local/include)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)
  endif (IS_DIRECTORY /usr/local/include)
endif (IS_DIRECTORY /usr/local)

#---------------------------------------------------------------------

# Normalize the build type capitalization
if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
  if ("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
  endif ("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
  if ("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
  endif ("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
endif(CMAKE_BUILD_TYPE)
# Stash the build type so we can set up a drop-down menu in the gui
if(NOT BRLCAD_IS_SUBBUILD)
  set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "BRL-CAD high level build configuration")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" Debug Release)
endif(NOT BRLCAD_IS_SUBBUILD)
if(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
else(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(CMAKE_BUILD_TYPE)
endif(CMAKE_CONFIGURATION_TYPES)

#---------------------------------------------------------------------
# The location in which to install BRL-CAD.  Only do this if
# CMAKE_INSTALL_PREFIX hasn't been set already, to try and allow
# parent builds (if any) some control.

# TODO - generator expressions may be a way to replace some of this logic (or
# improve it) by letting us ditch the logic tracking whether the path is
# initialized to default - that's used to know whether or not it is safe to
# override CMAKE_INSTALL_PREFIX based on build type.  If we use the generator
# expressions, we won't have to be concerned about that - problem is that
# change will require updating all the install commands in the build target
# functions.  Third party logic will probably need to shift over to the new
# ExternalProject_Add approach first as well - that way unmodified install
# commands in 3rd party CMake files will be moot.
#
# if (NOT CMAKE_INSTALL_PREFIX)
# set(CMAKE_INSTALL_PREFIX "/usr/brlcad/")
#    if (NOT MSVC)
#       set(gen_exp $<IF:$<CONFIG:Debug>,dev-${BRLCAD_VERSION}/,$<IF:$<CONFIG:Release>,rel-${BRLCAD_VERSION}/${BRLCAD_VERSION},>>)
#    else (NOT MSVC)
#       set(gen_exp)
#        if(CMAKE_CL_64)
#          set(CMAKE_INSTALL_PREFIX "C:/Program Files/BRL-CAD ${BRLCAD_VERSION}")
#        else(CMAKE_CL_64)
#          set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/BRL-CAD ${BRLCAD_VERSION}")
#        endif(CMAKE_CL_64)
#    endif (NOT MSVC)
# endif (NOT CMAKE_INSTALL_PREFIX)
# install(TARGETS mged DESTINATION ${gen_exp}${BIN_DIR})
#
# Another (maybe better) option instead of generators might be to use the CONFIGURATIONS
# option in our macros and the ExternalProject_Add management:
# https://cmake.org/cmake/help/latest/command/install.html
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    if("${CMAKE_BUILD_TYPE}" MATCHES "Release")
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/rel-${BRLCAD_VERSION}")
    else("${CMAKE_BUILD_TYPE}" MATCHES "Release")
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}")
    endif("${CMAKE_BUILD_TYPE}" MATCHES "Release")
  else(NOT CMAKE_CONFIGURATION_TYPES)
    if(MSVC)
      if(CMAKE_CL_64)
	set(CMAKE_INSTALL_PREFIX "C:/Program Files/BRL-CAD ${BRLCAD_VERSION}")
      else(CMAKE_CL_64)
	set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/BRL-CAD ${BRLCAD_VERSION}")
      endif(CMAKE_CL_64)
    else(MSVC)
      set(CMAKE_INSTALL_PREFIX "/usr/brlcad/dev-${BRLCAD_VERSION}")
    endif(MSVC)
  endif(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "BRL-CAD install prefix" FORCE)
  set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
set(BRLCAD_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE STRING "BRL-CAD install prefix")
mark_as_advanced(BRLCAD_PREFIX)
if (DEFINED CMAKE_INSTALL_PREFIX AND NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif (DEFINED CMAKE_INSTALL_PREFIX AND NOT DEFINED CMAKE_INSTALL_PREFIX)


# If we've a Release build with a Debug path or vice versa, warn about
# it.  A "make install" of a Release build into a dev install
# directory or vice versa is going to result in an install that
# doesn't respect BRL-CAD standard naming conventions.
if("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
  message(FATAL_ERROR "\nInstallation directory (CMAKE_INSTALL_PREFIX) is set to /usr/brlcad/dev-${BRLCAD_VERSION}, but build type is set to Release!\n")
endif("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/dev-${BRLCAD_VERSION}")
if("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")
  message(FATAL_ERROR "\nInstallation directory (CMAKE_INSTALL_PREFIX) is set to /usr/brlcad/rel-${BRLCAD_VERSION}, but build type is set to Debug!\n")
endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND "${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/brlcad/rel-${BRLCAD_VERSION}")

# If CMAKE_INSTALL_PREFIX is "/usr", be VERY noisy about it - a make install in
# this location is dangerous/destructive on some systems.
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT BRLCAD_ALLOW_INSTALL_TO_USR)
  message(WARNING "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}\nIt is STRONGLY recommended that you DO NOT install BRL-CAD into /usr as BRL-CAD provides several libraries that may conflict with other libraries (e.g. librt, libbu, libbn) on certain system configurations.\nSince our libraries predate all those that we're known to conflict with and are at the very core of our geometry services and project heritage, we have no plans to change the names of our libraries at this time.\nINSTALLING INTO /usr CAN MAKE A SYSTEM COMPLETELY UNUSABLE.  If you choose to continue installing into /usr, you do so entirely at your own risk.  You have been warned.\n}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  if(SLEEP_EXEC)
    execute_process(COMMAND ${SLEEP_EXEC} 15)
  endif(SLEEP_EXEC)
  message(FATAL_ERROR "If you wish to proceed using /usr as your prefix, define BRLCAD_ALLOW_INSTALL_TO_USR=1 for CMake")
endif("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr" AND NOT BRLCAD_ALLOW_INSTALL_TO_USR)

#------------------------------------------------------------------------------
# Now that we know the install prefix, generate the binary for calculating
# and reporting time deltas
generate_dreport()

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

# TODO - once we've shifted to function invocation rather than having
# our logic execute on load, this "include" step will probably be
# consolidated into one large include...
include(RPath_Setup)
cmake_set_rpath()

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
# On Windows, we can't set word size at CMake configure time - the
# compiler chosen at the beginning dictates the result.  Mark as
# advanced in that situation.
if(MSVC)
  mark_as_advanced(BRLCAD_WORD_SIZE)
endif(MSVC)

# calculate the size of a pointer if we haven't already
CHECK_TYPE_SIZE("void *" CMAKE_SIZEOF_VOID_P)

# still not defined?
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
    endif(NOT ${CMAKE_WORD_SIZE} MATCHES "64BIT")
    add_definitions("-D_WIN64")
  else(CMAKE_CL_64)
    set(CMAKE_SIZEOF_VOID_P 4)
    if(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
      set(CMAKE_WORD_SIZE "32BIT")
      if(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
	message(WARNING "Selected MSVC compiler is 32BIT - setting word size to 32BIT.  To perform a 64BIT MSVC build, select the 64BIT MSVC CMake generator.")
	set(BRLCAD_WORD_SIZE "32BIT" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
    endif(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
  endif(CMAKE_CL_64)
endif(MSVC)

if (APPLE)
  if (${CMAKE_WORD_SIZE} MATCHES "32BIT")
    set(CMAKE_OSX_ARCHITECTURES "i386" CACHE STRING "Building for i386" FORCE)
  endif (${CMAKE_WORD_SIZE} MATCHES "32BIT")
endif (APPLE)

CONFIG_H_APPEND(BRLCAD "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")

# OpenBSD doesn't define __WORD_SIZE
if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  CONFIG_H_APPEND(BRLCAD "#ifndef __WORDSIZE\n#  define __WORDSIZE 32\n#endif\n")
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")
if(${CMAKE_WORD_SIZE} MATCHES "64BIT")
  CONFIG_H_APPEND(BRLCAD "#ifndef __WORDSIZE\n#  define __WORDSIZE 64\n#endif\n")
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT")

# Based on what we are doing, we may need to constrain our search paths
#
# NOTE: Ideally we would set a matching property for 32 bit paths
# on systems that default to 64 bit - as of 2.8.8 CMake doesn't yet
# support FIND_LIBRARY_USE_LIB32_PATHS.  There is a bug report on the
# topic here: http://www.cmake.org/Bug/view.php?id=11260
#
if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
else(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")

# One of the problems with 32/64 building is we need to search anew
# for 64 bit libs after a 32 bit configure, or vice versa.
if(PREVIOUS_CONFIGURE_TYPE)
  if(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
    include(ResetCache)
    RESET_CACHE_file()
  endif(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
endif(PREVIOUS_CONFIGURE_TYPE)

set(PREVIOUS_CONFIGURE_TYPE ${CMAKE_WORD_SIZE} CACHE STRING "Previous configuration word size" FORCE)
mark_as_advanced(PREVIOUS_CONFIGURE_TYPE)

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
#
# Now we define the various options for BRL-CAD - ways to enable and
# disable features, select which parts of the system to build, etc.
# As much as possible, sane default options are either selected or
# detected.  Because documentation is autogenerated for BRL-CAD
# options, be sure to initialize the file.
set(CONFIG_OPT_STRING "CONFIGURATION OPTIONS\n---------------------\n")
file(WRITE "${CMAKE_BINARY_DIR}/BRLCAD_OPTIONS" "${CONFIG_OPT_STRING}")

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

# Build shared libs by default.  Mark this as advanced - turning off
# ALL shared library building is unlikely to result in a working build
# and is not a typical configuration.  Note that turning this option off
# will not disable libraries specifically added as SHARED.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
mark_as_advanced(BUILD_SHARED_LIBS)

# Build static libs by default unless we're debugging. Note: this
# option will not disable libraries specifically added as STATIC even
# when OFF.  For multi-configuration options the build type determination
# is made at build time, so just default to "ON".
if(NOT CMAKE_BUILD_TYPE)
  set(CBT "MULTICONFIG")
else(NOT CMAKE_BUILD_TYPE)
  set(CBT "${CMAKE_BUILD_TYPE}")
endif(NOT CMAKE_BUILD_TYPE)
cmake_dependent_option(BUILD_STATIC_LIBS "Build static libraries" ON "CMAKE_CONFIGURATION_TYPES OR NOT ${CBT} STREQUAL Debug" OFF)

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
set(BRLCAD_BUNDLED_LIBS_DESCRIPTION "
Enables compilation of all 3rd party sources that are provided within a BRL-CAD
source distribution.  If used this option sets all other 3rd party library
build flags to ON by default.  However, that setting can be overridden by
manually setting individual variables. Default is \"AUTO\" - 3rd party sources
are compiled only if they are not detected as being available and functioning
as expected.
")

if(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "BUNDLED")
else(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "AUTO")
endif(MSVC)
BRLCAD_OPTION(BRLCAD_BUNDLED_LIBS ${BRLCAD_BUNDLED_LIBS_DEFAULT}
  TYPE ABS
  ALIASES ENABLE_ALL
  DESCRIPTION BRLCAD_BUNDLED_LIBS_DESCRIPTION)
if(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")
  message(WARNING "Unknown value BRLCAD_BUNDLED_LIBS supplied for BRLCAD_BUNDLED_LIBS (${BRLCAD_BUNDLED_LIBS}) - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, BUNDLED and SYSTEM")
  set(BRLCAD_BUNDLED_LIBS "AUTO" CACHE STRING "Build bundled libraries." FORCE)
endif(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")

# Single flag for disabling GUI instead of 5.
option(BRLCAD_ENABLE_MINIMAL "Skip GUI support and docs. Faster builds." OFF)
mark_as_advanced(BRLCAD_ENABLE_MINIMAL)

if(BRLCAD_ENABLE_MINIMAL)
  set(BRLCAD_ENABLE_OPENGL "OFF")
  set(BRLCAD_ENABLE_X11 "OFF")
  set(BRLCAD_ENABLE_TK "OFF")
  set(BRLCAD_ENABLE_QT "OFF")
  set(BRLCAD_ENABLE_AQUA "OFF")
  set(BRLCAD_EXTRADOCS "OFF")
endif(BRLCAD_ENABLE_MINIMAL)

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building. Not currently working - needs work in at least Tk
# CMake logic (probably more), and the display manager/framebuffer codes are known to depend
# on either GLX or WGL specifically in their current forms.
option(BRLCAD_ENABLE_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)
mark_as_advanced(BRLCAD_ENABLE_AQUA)

# Install example BRL-CAD Geometry Files
option(BRLCAD_INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)

# test for X11 on all platforms since we don't know when/where we'll find it, unless
# we've indicated we *don't* want an X11 build
if(NOT BRLCAD_ENABLE_AQUA AND NOT BRLCAD_ENABLE_MINIMAL)
  include(FindX11)
  if (X11_Xrender_FOUND)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_XRENDER 1\n")
  endif (X11_Xrender_FOUND)
endif(NOT BRLCAD_ENABLE_AQUA AND NOT BRLCAD_ENABLE_MINIMAL)

# make sure Xi is included in the list of X11 libs
# (Xext is automatically added during FindX11)
if(X11_Xi_FOUND)
    set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_Xi_LIB})
endif(X11_Xi_FOUND)

# Set whether X11 is enabled or disabled by default
if(WIN32)
  # even if there is x11, we default to native
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
elseif(BRLCAD_ENABLE_AQUA)
  # aqua implies no X11
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
else(WIN32)
  # make everywhere else depend on whether we found a suitable X11
  # X11_Xext_LIB AND X11_Xi_LIB AND
  if(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
    option(BRLCAD_ENABLE_X11 "Use X11." ON)
  else(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
    option(BRLCAD_ENABLE_X11 "Use X11." OFF)
  endif(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
endif(WIN32)
mark_as_advanced(BRLCAD_ENABLE_X11)

# if X11 is enabled, make sure aqua is off
if(BRLCAD_ENABLE_X11)
  set(BRLCAD_ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
  set(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif(BRLCAD_ENABLE_X11)
mark_as_advanced(OPENGL_USE_AQUA)

# Enable/disable features requiring the Tcl/Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tcl/Tk

option(BRLCAD_ENABLE_TCL "Enable features requiring the Tcl toolkit" ON)
cmake_dependent_option(BRLCAD_ENABLE_TK "Enable features requiring the Tk toolkit" ON "BRLCAD_ENABLE_TCL" OFF)
mark_as_advanced(BRLCAD_ENABLE_TCL BRLCAD_ENABLE_TK)
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(BRLCAD_ENABLE_TK OFF)
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
if(BRLCAD_ENABLE_X11)
  set(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
endif(BRLCAD_ENABLE_X11)

# Enable features requiring OPENGL
# Be smart about this - if we don't have X11 or Aqua and we're
# not on Windows, we're non-graphical and that means OpenGL is
# a no-go.  The Windows version would have to be some sort of
# option for the WIN32 graphics layer?  Should probably think
# about that... for now, on Win32 don't try OpenGL if Tk is
# off.  That'll hold until we get a non-Tk based GUI - then
# setting non-graphical on Windows will take more thought.
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(OPENGL_FOUND OFF)
  set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - NOT BRLCAD_ENABLE_X11 and NOT BRLCAD_ENABLE_AQUA" FORCE)
else(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  include(FindGL)
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)

set(BRLCAD_ENABLE_OPENGL_DESCRIPTION "
Enable support for OpenGL based Display Managers in BRL-CAD.
Default depends on whether OpenGL is successfully detected -
if it is, default is to enable.
")
BRLCAD_OPTION(BRLCAD_ENABLE_OPENGL ${OPENGL_FOUND}
  TYPE BOOL
  ALIASES ENABLE_OPENGL
  DESCRIPTION BRLCAD_ENABLE_OPENGL_DESCRIPTION)

if(BRLCAD_ENABLE_AQUA)
  set(OPENGL_USE_AQUA ON CACHE STRING "Aqua enabled - use Aqua OpenGL" FORCE)
endif(BRLCAD_ENABLE_AQUA)

# Enable features requiring Bullet Physics SDK
option(BRLCAD_ENABLE_BULLET "Enable features requiring the Bullet Physics Library" OFF)
if(BRLCAD_ENABLE_BULLET)
  message("Bullet support is not currently working - disabling.")
  set(BRLCAD_ENABLE_BULLET OFF CACHE BOOL "Currently broken" FORCE)
endif(BRLCAD_ENABLE_BULLET)
mark_as_advanced(BRLCAD_ENABLE_BULLET)

# Enable features requiring GDAL geospatial library
option(BRLCAD_ENABLE_GDAL "Enable features requiring the Geospatial Data Abstraction Library" ON)
mark_as_advanced(BRLCAD_ENABLE_GDAL)

# Enable features requiring STEPcode library
option(BRLCAD_ENABLE_STEP "Enable features requiring the STEP support libraries" ON)
mark_as_advanced(BRLCAD_ENABLE_STEP)

# Enable features requiring Qt
option(BRLCAD_ENABLE_QT "Enable features requiring Qt" OFF)
mark_as_advanced(BRLCAD_ENABLE_QT)
if (BRLCAD_ENABLE_QT)
  # TODO - try COMPONENTS search: https://blog.kitware.com/cmake-finding-qt5-the-right-way/
  find_package(Qt5Widgets QUIET)
  if(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
    message("QT interface requested, but QT5 is not found - disabling")
    set(BRLCAD_ENABLE_QT OFF)
  endif(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
endif (BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Widgets_DIR)
mark_as_advanced(Qt5Core_DIR)
mark_as_advanced(Qt5Gui_DIR)

# Enable features requiring OpenSceneGraph
option(BRLCAD_ENABLE_OSG "Enable features requiring OpenSceneGraph" OFF)
mark_as_advanced(BRLCAD_ENABLE_OSG)
if(BRLCAD_ENABLE_OSG)
  if(APPLE AND NOT BRLCAD_ENABLE_AQUA)
    set(OSG_WINDOWING_SYSTEM "X11" CACHE STRING "Use X11" FORCE)
  endif(APPLE AND NOT BRLCAD_ENABLE_AQUA)
endif(BRLCAD_ENABLE_OSG)

# Enable features requiring GCT
option(BRLCAD_ENABLE_GCT "Enable features requiring GCT" ON)
mark_as_advanced(BRLCAD_ENABLE_GCT)
if(NOT BRLCAD_ENABLE_GCT)
  CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DISABLE_GCT 1\n")
endif(NOT BRLCAD_ENABLE_GCT)

# Enable features requiring OpenCL
option(BRLCAD_ENABLE_OPENCL "Enable features requiring OpenCL" OFF)
mark_as_advanced(BRLCAD_ENABLE_OPENCL)
if(BRLCAD_ENABLE_OPENCL)
  find_package(OpenCL)
  if(NOT OPENCL_FOUND)
    message("OpenCL enablement requested, but OpenCL is not found - disabling")
    set(BRLCAD_ENABLE_OPENCL OFF)
  endif(NOT OPENCL_FOUND)
endif(BRLCAD_ENABLE_OPENCL)

# Enable features requiring OpenVDB
option(BRLCAD_ENABLE_OPENVDB "Enable features requiring OpenVDB" OFF)
mark_as_advanced(BRLCAD_ENABLE_OPENVDB)
if(BRLCAD_ENABLE_OPENVDB)
  find_package(OpenVDB)
  if(NOT OpenVDB_FOUND)
    message("OpenVDB enablement requested, but OpenVDB is not found - disabling")
    set(BRLCAD_ENABLE_OPENVDB OFF)
  endif(NOT OpenVDB_FOUND)
endif(BRLCAD_ENABLE_OPENVDB)

# Enable experimental support for binary attributes
option(BRLCAD_ENABLE_BINARY_ATTRIBUTES "Enable support for binary attributes" OFF)
mark_as_advanced(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  CONFIG_H_APPEND(BRLCAD "#define USE_BINARY_ATTRIBUTES 1\n")
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

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
Default is \"ON\", and should only be disabled for read-only render
work where performance is critical.
")
BRLCAD_OPTION(BRLCAD_ENABLE_RUNTIME_DEBUG ON
  TYPE BOOL
  ALIASES ${BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES}
  DESCRIPTION BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION)
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
  CONFIG_H_APPEND(BRLCAD "/*Define to not do anything for macros that only bomb on a fatal error. */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_BOMBING_MACROS 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not perform magic number checking */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_MAGIC_CHECKING 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not provide run-time debug facilities via RTG.debug */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_DEBUG_CHECKING 1\n")
endif(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)

# Enable debug flags during compilation - we always want to use these
# unless explicitly told not to.
set(BRLCAD_FLAGS_DEBUG_DESCRIPTION "
Add compiler flags to aid in program debugging.  Defaults to ON.
")
BRLCAD_OPTION(BRLCAD_FLAGS_DEBUG ON
  TYPE BOOL
  ALIASES ENABLE_DEBUG ENABLE_FLAGS_DEBUG ENABLE_DEBUG_FLAGS
  DESCRIPTION BRLCAD_FLAGS_DEBUG_DESCRIPTION)

# A variety of debugging messages in the code key off of the DEBUG
# definition - set it according to whether we're using debug flags.
if(BRLCAD_FLAGS_DEBUG)
  CONFIG_H_APPEND(BRLCAD "#define DEBUG 1\n")
endif(BRLCAD_FLAGS_DEBUG)

# Build with compiler warning flags
set(BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION "
Use extra compiler warning flags when compiling C/C++ code.  Defaults to ON.
")
BRLCAD_OPTION(BRLCAD_ENABLE_COMPILER_WARNINGS ON
  TYPE BOOL
  ALIASES ENABLE_WARNINGS ENABLE_COMPILER_WARNINGS
  DESCRIPTION BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_COMPILER_WARNINGS)

# Enable/disable strict compiler settings - these are used for building
# BRL-CAD by default, but not src/other code.  Always used for BRL-CAD
# code unless the NO_STRICT option is specified when defining a target
# with BRLCAD_ADDEXEC or BRLCAD_ADDLIB.  If only C++ files in a target
# are not compatible with strict, the NO_STRICT_CXX option can be used.
set(BRLCAD_ENABLE_STRICT_DESCRIPTION "
Causes all compilation warnings for C code to be treated as errors.  This is now
the default for BRL-CAD source code, and developers should address issues
discovered by these flags whenever possible rather than disabling strict
mode.
")
BRLCAD_OPTION(BRLCAD_ENABLE_STRICT ON
  TYPE BOOL
  ALIASES ENABLE_STRICT ENABLE_STRICT_COMPILE ENABLE_STRICT_COMPILE_FLAGS
  DESCRIPTION BRLCAD_ENABLE_STRICT_DESCRIPTION)
if(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_STRICT)
endif(BRLCAD_ENABLE_STRICT)

# Build with compiler optimization flags.  This should normally be on for
# release builds and off otherwise, unless the user specifically enables it.
# For multi-config build tools, this is managed on a per-configuration basis.
if(CMAKE_BUILD_TYPE)
  cmake_dependent_option(BRLCAD_OPTIMIZED_BUILD "Enable optimized build flags" ON "${CMAKE_BUILD_TYPE} STREQUAL Release" OFF)
else(CMAKE_BUILD_TYPE)
  # Note: the cmake_dependent_option test doesn't work if CMAKE_BUILD_TYPE isn't set.
  option(BRLCAD_OPTIMIZED_BUILD "Enable optimized build flags" OFF)
endif(CMAKE_BUILD_TYPE)
mark_as_advanced(BRLCAD_OPTIMIZED_BUILD)

# Build with full compiler lines visible by default (won't need make
# VERBOSE=1) on command line
option(BRLCAD_ENABLE_VERBOSE_PROGRESS "verbose output" OFF)
mark_as_advanced(BRLCAD_ENABLE_VERBOSE_PROGRESS)
if(BRLCAD_ENABLE_VERBOSE_PROGRESS)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(BRLCAD_ENABLE_VERBOSE_PROGRESS)

# Build with profile-guided optimization support.  this requires a
# two-pass compile, once with BRLCAD_PGO=ON on a location that did not
# exist beforehand (specified via the PGO_PATH environment variable),
# and again to use profiling metrics captured on "typical" operations
# and data.  By default, path is BUILDDIR/profiling

option(BRLCAD_PGO "Enable profile-guided optimization (set PGO_PATH environment variable)")
mark_as_advanced(BRLCAD_PGO)

#====== ALL CXX COMPILE ===================
# Build all C and C++ files with a C++ compiler
set(ENABLE_ALL_CXX_COMPILE_DESCRIPTION "
Build all C and C++ files with a C++ compiler.  Defaults to OFF.

EXPERIMENTAL!
")
BRLCAD_OPTION(ENABLE_ALL_CXX_COMPILE OFF
  TYPE BOOL
  ALIASES ENABLE_ALL_CXX
  DESCRIPTION ENABLE_ALL_CXX_COMPILE_DESCRIPTION)
mark_as_advanced(ENABLE_ALL_CXX_COMPILE)

# Build with coverage enabled
option(BRLCAD_ENABLE_COVERAGE "Build with coverage enabled" OFF)
mark_as_advanced(BRLCAD_ENABLE_COVERAGE)

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
mark_as_advanced(BRLCAD_ENABLE_SMP)
if(BRLCAD_ENABLE_SMP)
  CONFIG_H_APPEND(BRLCAD "#define PARALLEL 1\n")
endif(BRLCAD_ENABLE_SMP)

if(BRLCAD_HEADERS_OLD_COMPAT)
  add_definitions(-DEXPOSE_FB_HEADER)
  add_definitions(-DEXPOSE_DM_HEADER)
endif(BRLCAD_HEADERS_OLD_COMPAT)

#----------------------------------------------------------------------
# Some generators in CMake support generating folders in IDEs for
# organizing build targets.  We want to use them if they are there.
if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif(MSVC)

#----------------------------------------------------------------------
# There are extra documentation files available requiring DocBook
# They are quite useful in graphical interfaces, but also add considerably
# to the overall build time.  If necessary BRL-CAD provides its own
# xsltproc (see src/other/xmltools), so the html and man page
# outputs are always potentially available.  PDF output, on the other hand,
# needs Apache FOP.  FOP is not a candidate for bundling with BRL-CAD for
# a number of reasons, so we simply check to see if it is present and set
# the options accordingly.

# Do we have the environment variable set locally?
if(NOT "$ENV{APACHE_FOP}" STREQUAL "")
  set(APACHE_FOP "$ENV{APACHE_FOP}")
endif(NOT "$ENV{APACHE_FOP}" STREQUAL "")
if(NOT APACHE_FOP)
  find_program(APACHE_FOP fop DOC "path to the exec script for Apache FOP")
endif(NOT APACHE_FOP)
mark_as_advanced(APACHE_FOP)
# We care about the FOP version, unfortunately - find out what we have.
if(APACHE_FOP)
  execute_process(COMMAND ${APACHE_FOP} -v OUTPUT_VARIABLE APACHE_FOP_INFO ERROR_QUIET)
  string(REGEX REPLACE "FOP Version ([0-9\\.]*)" "\\1" APACHE_FOP_VERSION_REGEX "${APACHE_FOP_INFO}")
  if(APACHE_FOP_VERSION_REGEX)
    string(STRIP ${APACHE_FOP_VERSION_REGEX} APACHE_FOP_VERSION_REGEX)
  endif(APACHE_FOP_VERSION_REGEX)
  if(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
    message("-- Found Apache FOP: version ${APACHE_FOP_VERSION_REGEX}")
    set(APACHE_FOP_VERSION ${APACHE_FOP_VERSION_REGEX} CACHE STRING "Apache FOP version" FORCE)
    mark_as_advanced(APACHE_FOP_VERSION)
  endif(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
endif(APACHE_FOP)

# Toplevel variable that controls all DocBook based documentation.  Key it off
# of what target level is enabled.
if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "ON")
else(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "OFF")
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
set(BRLCAD_EXTRADOCS_DESCRIPTION "
The core option that enables and disables building of BRL-CAD's
DocBook based documentation (includes manuals and man pages for
commands, among other things).  Defaults to ON, but only HTML and MAN
formats are enabled by default - PDF must be enabled separately by use
of this option or one of its aliases.  Note that you may set
environment variable APACHE_FOP to point to your locally installed fop
executable file (which on Linux is usually a shell script with 0755
permissions).
")
BRLCAD_OPTION(BRLCAD_EXTRADOCS ${EXTRADOCS_DEFAULT}
  TYPE BOOL
  ALIASES ENABLE_DOCS ENABLE_EXTRA_DOCS ENABLE_DOCBOOK
  DESCRIPTION BRLCAD_EXTRADOCS_DESCRIPTION)


# The HTML output is used in the graphical help browsers in MGED and Archer,
# as well as being the most likely candidate for external viewers. Turn this
# on unless explicitly instructed otherwise by the user or all extra
# documentation is disabled.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_HTML "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_HTML)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PHP "Build MAN page output from DocBook documentation" OFF "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PHP)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PPT "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PPT)

# Normally, we'll turn on man page output by default, but there is
# no point in doing man page output for a Visual Studio build - the
# files aren't useful and it *seriously* increases the target build
# count/build time.  Conditionalize on the CMake MSVC variable NOT
# being set.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_MAN "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS;NOT MSVC" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_MAN)

# Don't do PDF by default because it's pretty expensive, and hide the
# option unless the tools to do it are present.
set(BRLCAD_EXTRADOCS_PDF_DESCRIPTION "
Option that enables building of BRL-CAD's DocBook PDF-based documentation
(includes manuals and man pages for commands, among
other things.) Defaults to OFF.
Note that you may set environment variable APACHE_FOP
to point to your locally installed fop executable file (which on Linux is
usually a shell script with 0755 permissions).
")
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PDF "Build PDF output from DocBook documentation" OFF "BRLCAD_EXTRADOCS;APACHE_FOP" OFF)

# Provide an option to enable/disable XML validation as part
# of the DocBook build - sort of a "strict flags" mode for DocBook.
# By default, this will be enabled when extra docs are built and
# the toplevel BRLCAD_ENABLE_STRICT setting is enabled.
# Unfortunately, Visual Studio 2010 seems to have issues when we
# enable validation on top of everything else... not clear why,
# unless build target counts >1800 are beyond MSVC's practical
# limit.  Until we either find a resolution or a way to reduce
# the target count on MSVC, disable validation there.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_VALIDATE "Perform validation for DocBook documentation" ON "BRLCAD_EXTRADOCS;BRLCAD_ENABLE_STRICT" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_VALIDATE)


#----------------------------------------------------------------------
# Load Doxygen related CMake macros
include(Doxygen)

#----------------------------------------------------------------------
# Load various wrapper macros for checking libraries, headers and
# functions, some in use by src/other build logic
include(BRLCAD_CheckFunctions)


# *******************************************************************
if(BRLCAD_PRINT_MSGS)
  message("***********************************************************")
  message("*             Stage 2 of 9 - Check for Programs           *")
  message("***********************************************************")
endif(BRLCAD_PRINT_MSGS)

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

# Check whether the compiler supports __attribute__((flatten))
check_c_source_compiles("__attribute__((flatten)) int *func(int *v){(*v)-=1; return v;} __attribute((flatten)) int main(int argc, char *argv[]) {int v = 1; int *vp = func(&v); return *vp;}" HAVE_FLATTEN_ATTRIBUTE)
if(HAVE_FLATTEN_ATTRIBUTE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_FLATTEN_ATTRIBUTE 1\n")
endif(HAVE_FLATTEN_ATTRIBUTE)

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
    endif (PROBE_PTHREAD_H_CLOCKID_T)
  endif (NOT PROBE_PTHREAD_H)

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
if(CMAKE_THREAD_LIBS_INIT)
  message("Using threading library: ${CMAKE_THREAD_LIBS_INIT}")
else(CMAKE_THREAD_LIBS_INIT)
  message("Using threading from system library")
endif(CMAKE_THREAD_LIBS_INIT)

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

# If we have GetProcessTimes, we need it instead of clock() for CPU time.
# https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/clock
check_c_source_compiles("
#include <windows.h>
int main() {
FILETIME a,b,c,d;
(void)GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d);
return 0;
}
" HAVE_GETPROCESSTIMES)
if(HAVE_GETPROCESSTIMES)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_GETPROCESSTIMES 1\n")
endif(HAVE_GETPROCESSTIMES)

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

# At the start, clear the src/other subdirs list so repeated
# configures will correctly add the required directories
set(SRC_OTHER_ADDED_DIRS "" CACHE STRING "initialize 3rd party sub-directories list" FORCE)
mark_as_advanced(SRC_OTHER_ADDED_DIRS)

function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# Load some CMake macros to handle the special case of third party libraries.
include(ThirdParty)

# Add misc/tools for tools that are used in BRL-CAD's build process
# but are not otherwise usable in BRL-CAD (due to licensing, design
# intent, etc.)  misc/tools must be handled before src/other because
# some of src/other's projects need tools from misc/tools.
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

# Restore BRL-CAD configurations, if necessary
if(NOT ENABLE_ALL_CONFIG_TYPES)
  if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
  endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
endif(NOT ENABLE_ALL_CONFIG_TYPES)



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
  string(REPLACE "\${BUILD_TYPE}" "----BUILD_TYPE----" CMAKE_INSTALL_PREFIX_CFG "${CMAKE_INSTALL_PREFIX}")
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

  configure_file("${BRLCAD_CMAKE_DIR}/source_archive_setup.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake" @ONLY)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake")

  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BRL-CAD - a powerful cross-platform open source solid modeling system")
  set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
  set(CPACK_PACKAGE_VERSION_MAJOR ${BRLCAD_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${BRLCAD_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${BRLCAD_VERSION_PATCH})

  # By default, we want debugging information
  set(CPACK_STRIP_FILES FALSE)

  # If we're not on Windows, set the install prefix and create
  # TGZ and TBZ2 packages by default
  if(NOT WIN32)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_GENERATOR TGZ TBZ2)
  endif(NOT WIN32)

  # If we have RPMBUILD and it's not explicitly disabled, assume we're making an RPM.
  find_program(RPMBUILD_EXEC rpmbuild)
  mark_as_advanced(RPMBUILD_EXEC)
  if(RPMBUILD_EXEC AND NOT CPACK_RPM_SKIP)

    # Build an RPM by default if we have the tool.  Out of the box, just
    # build a "vanilla" RPM.  For situations where we want more detail
    # and versioning of the RPM, set the variable CMAKE_RPM_VERSION
    # to the numerical "version" of the RPM: 1, 2, etc..

    # Since RPM packages present a particular problem with bad umask
    # settings and RPM package building is enabled, raise the issue again
    # with a longer wait time.
    if (NOT UMASK_OK)
      message(" ")
      message(WARNING "umask is set to ${umask_curr} and RPM package building is enabled - this is not a 'standard' umask setting for BRL-CAD RPM packages.  Double check that these umask permissions will have the desired results when installed - RPM packages can impact permissions on system directories such as /usr\nIf the umask settings need to be changed, it is recommended that the build directory be cleared and cmake re-run after the umask setting has been changed.")
      if(SLEEP_EXEC)
	execute_process(COMMAND ${SLEEP_EXEC} 5)
      endif(SLEEP_EXEC)
    endif (NOT UMASK_OK)

    set(CPACK_GENERATOR ${CPACK_GENERATOR} RPM)
    set(CPACK_RPM_PACKAGE_LICENSE "LGPL 2.1")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")

    # We do NOT want to strip the binaries added to the RPM -
    # see https://cmake.org/Wiki/CMake:CPackPackageGenerators and
    # https://public.kitware.com/Bug/view.php?id=7435
    set(CPACK_RPM_SPEC_INSTALL_POST /bin/true)

    if(CPACK_RPM_VERSION)
      if(DEFINED BRLCAD_VERSION_AMEND)
	set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}_${BRLCAD_VERSION_AMEND}")
      else(DEFINED BRLCAD_VERSION_AMEND)
	set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}")
      endif(DEFINED BRLCAD_VERSION_AMEND)

      # If we've got a Redhat release, include some info about the
      # specific release in the name.  Otherwise, just go generic.
      if(EXISTS /etc/redhat-release)
	file(READ /etc/redhat-release REDHAT_RELEASE)
	string(REGEX MATCH "[0-9]+" REDHAT_VERSION ${REDHAT_RELEASE})
	string(REGEX MATCH "Enterprise Linux" LINUX_DIST_TYPE ${REDHAT_RELEASE})
	if(LINUX_DIST_TYPE)
	  set(LINUX_DIST_TYPE "el")
	else(LINUX_DIST_TYPE)
	  set(LINUX_DIST_TYPE "rh")
	endif(LINUX_DIST_TYPE)
	set(CPACK_RPM_PACKAGE_RELEASE ${CPACK_RPM_VERSION}.${LINUX_DIST_TYPE}${REDHAT_VERSION})
      else(EXISTS /etc/redhat-release)
	set(CPACK_RPM_PACKAGE_RELEASE ${CPACK_RPM_VERSION})
      endif(EXISTS /etc/redhat-release)

    endif(CPACK_RPM_VERSION)
  endif(RPMBUILD_EXEC AND NOT CPACK_RPM_SKIP)

  if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_x86")
  else(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
  endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  if(CPACK_RPM_PACKAGE_RELEASE)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}_${CPACK_RPM_PACKAGE_RELEASE}")
  endif(CPACK_RPM_PACKAGE_RELEASE)

  if(WIN32)
    find_package(NSIS)
    if (NSIS_FOUND)
      set(CPACK_GENERATOR ${CPACK_GENERATOR} NSIS)
      set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD")
      set(CPACK_NSIS_INSTALL_DIRECTORY "BRL-CAD ${BRLCAD_VERSION}")
      set(CPACK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
      set(CPACK_DATA_DIR "${DATA_DIR}")
      set(CPACK_DOC_DIR "${DOC_DIR}")
      # There is a bug in NSIS that does not handle full unix paths properly. Make
      # sure there is at least one set of four (4) backslashes.
      set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\brlcad.ico")
      set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\uninstall.ico")
      set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
      set(CPACK_NSIS_DISPLAY_NAME "BRL-CAD")
      set(CPACK_NSIS_MODIFY_PATH ON)
    endif (NSIS_FOUND)
    find_package(Wix)
    if (Wix_FOUND)
      # User report that this version of the installer also supports
      # non-graphical installation with the /passive option.
      #
      # Note: for WiX, start menu shortcuts and desktop icons are handled with
      # properties set on targets.  (At the moment, this is not true for NSIS -
      # it uses entries in the misc/CMake/NSIS.template.in file.)
      #
      # If we need to get fancier about this, look at the following:
      # https://github.com/Kitware/CMake/blob/master/CMakeCPackOptions.cmake.in#L216
      # https://github.com/Kitware/CMake/tree/master/Utilities/Release/WiX
      set(CPACK_GENERATOR ${CPACK_GENERATOR} WIX)
      set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/misc/wix/License.rtf")
      set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_product.ico")
      set(CPACK_WIX_PROGRAM_MENU_FOLDER "BRL-CAD ${BRLCAD_VERSION}")
      set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_banner.bmp")
      set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_dialog.bmp")
    endif (Wix_FOUND)
    if (NOT CPACK_GENERATOR)
      # If nothing else, make a zip file
      set(CPACK_GENERATOR ZIP)
    endif (NOT CPACK_GENERATOR)
    if(CMAKE_CL_64)
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_win64")
      set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "BRL-CAD ${BRLCAD_VERSION} win64")
      # Use the setting from http://public.kitware.com/pipermail/cmake/2013-June/055000.html to
      # provide the correct default 64 bit directory with older CMake versions
      set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    else(CMAKE_CL_64)
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_win32")
      set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "BRL-CAD ${BRLCAD_VERSION} win32")
      set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD (32 Bit)")
    endif(CMAKE_CL_64)
  endif(WIN32)

  set(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "brlcad-${BRLCAD_VERSION}")
  set(CPACK_SOURCE_IGNORE_FILES "\\\\.svn/")

  configure_file("${BRLCAD_CMAKE_DIR}/BRLCAD_CPackOptions.cmake.in" "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake" @ONLY)
  DISTCLEAN("${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")
  set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")

  include(CPack)

  DISTCLEAN("${CMAKE_BINARY_DIR}/CPackConfig.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/DartConfiguration.tcl")

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
  DISTCLEAN("${CMAKE_BINARY_DIR}/BRLCAD_OPTIONS")
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
  if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    DISTCLEAN("${CMAKE_BINARY_DIR}/.ninja_log")
    DISTCLEAN("${CMAKE_BINARY_DIR}/.ninja_deps")
  endif("${CMAKE_GENERATOR}" MATCHES "Ninja")

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
  CREATE_DISTCHECK(odd_pathnames "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED" "1 Odd_ source dir ++" "1 Odd_ build dir ++" "1 Odd_ install dir ++")
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
file(READ "${BRLCAD_BINARY_DIR}/BRLCAD_OPTIONS" INSTALL_OPTS)
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
