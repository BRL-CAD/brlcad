#             C O M P I L E R F L A G S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2013 United States Government as represented by
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
###
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# To reduce verbosity in this file, determine up front which
# build configuration type (if any) we are using and stash
# the variables we want to assign flags to into a common
# variable that will be used for all routines.
macro(ADD_NEW_FLAG FLAG_TYPE NEW_FLAG CONFIG_LIST)
  if(${NEW_FLAG})
    if("${CONFIG_LIST}" STREQUAL "ALL")
      set(CMAKE_${FLAG_TYPE}_FLAGS "${CMAKE_${FLAG_TYPE}_FLAGS} ${${NEW_FLAG}}")
    elseif("${CONFIG_LIST}" STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE)
      set(CMAKE_${FLAG_TYPE}_FLAGS "${CMAKE_${FLAG_TYPE}_FLAGS} ${${NEW_FLAG}}")
    else("${CONFIG_LIST}" STREQUAL "ALL")
      foreach(CFG_TYPE ${CONFIG_LIST})
	set(VALID_CONFIG 1)
        if(CMAKE_CONFIGURATION_TYPES)
          list(FIND CMAKE_CONFIGURATION_TYPES "${CFG_TYPE}" VALID_CONFIG)
	else(CMAKE_CONFIGURATION_TYPES)
	  if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "${CFG_TYPE}")
	    set(VALID_CONFIG "-1")
	  endif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "${CFG_TYPE}")
	endif(CMAKE_CONFIGURATION_TYPES)
	if(NOT "${VALID_CONFIG}" STREQUAL "-1")
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE)
	  if(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
	    set(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE} "${CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE}} ${${NEW_FLAG}}")
	  else(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
	    set(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE} "${${NEW_FLAG}}")
	  endif(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
	endif(NOT "${VALID_CONFIG}" STREQUAL "-1")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif("${CONFIG_LIST}" STREQUAL "ALL")
  endif(${NEW_FLAG})
endmacro(ADD_NEW_FLAG)

# Convenience language based wrapper for calling the correct compiler
# flag test macro
macro(CHECK_COMPILER_FLAG FLAG_LANG NEW_FLAG RESULTVAR)
  if("${FLAG_LANG}" STREQUAL "C")
    CHECK_C_COMPILER_FLAG(${NEW_FLAG} ${RESULTVAR})
  endif("${FLAG_LANG}" STREQUAL "C")
  if("${FLAG_LANG}" STREQUAL "CXX")
    CHECK_CXX_COMPILER_FLAG(${NEW_FLAG} ${RESULTVAR})
  endif("${FLAG_LANG}" STREQUAL "CXX")
endmacro(CHECK_COMPILER_FLAG LANG NEW_FLAG RESULTVAR)


# Synopsis:  CHECK_FLAG(LANG flag [BUILD_TYPES type1 type2 ...] [GROUPS group1 group2 ...] [VARS var1 var2 ...] )
#
# CHECK_FLAG is BRL-CAD's core macro for C/C++ flag testing.
# The first value is the language to test (C or C++ currently).  The
# second entry is the flag (without preliminary dash).
#
# If the first two mandatory options are the only ones provided, a
# successful test of the flag will result in its being assigned to
# *all* compilations using the appropriate global C/C++ CMake
# variable.  If optional parameters are included, they tell the macro
# what to do with the test results instead of doing the default global
# assignment.  Options include assigning the flag to one or more of
# the variable lists associated with build types (e.g. Debug or
# Release), appending the variable to a string that contains a group
# of variables, or assigning the flag to a variable if that variable
# does not already hold a value.  The assignments are not mutually
# exclusive - any or all of them may be used in a given command.
#
# For example, to test a flag and add it to the C Debug configuration
# flags:
#
# CHECK_FLAG(C ggdb3 BUILD_TYPES Debug)
#
# To assign a C flag to a unique variable:
#
# CHECK_FLAG(C c99 VARS C99_FLAG)
#
# To do all assignments at once, for multiple configs and vars:
#
# CHECK_FLAG(C ggdb3
#                   BUILD_TYPES Debug Release
#                   GROUPS DEBUG_FLAGS
#                   VARS DEBUG1 DEBUG2)

include (CMakeParseArguments)
macro(CHECK_FLAG)
  # Set up some variables and names
  set(FLAG_LANG ${ARGV0})
  set(flag ${ARGV1})
  string(TOUPPER ${flag} UPPER_FLAG)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" UPPER_FLAG ${UPPER_FLAG})
  set(NEW_FLAG "-${flag}")

  # Start processing arguments
  if(${ARGC} LESS 3)

    # Handle default (global) case
    CHECK_COMPILER_FLAG(${FLAG_LANG} ${NEW_FLAG} ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      ADD_NEW_FLAG(${FLAG_LANG} NEW_FLAG ALL)
    endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)

  else(${ARGC} LESS 3)

    # Parse extra arguments
    CMAKE_PARSE_ARGUMENTS(FLAG "" "" "BUILD_TYPES;GROUPS;VARS" ${ARGN})

    # Iterate over listed Build types and append the flag to them if successfully tested.
    foreach(build_type ${FLAG_BUILD_TYPES})
      CHECK_COMPILER_FLAG(${FLAG_LANG} ${NEW_FLAG} ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
	ADD_NEW_FLAG(${FLAG_LANG} NEW_FLAG "${build_type}")
      endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    endforeach(build_type ${FLAG_BUILD_TYPES})

    # Append flag to a group of flags (this apparently needs to be
    # a string build, not a CMake list build.  Do this for all supplied
    # group variables.
    foreach(flag_group ${FLAG_GROUPS})
      CHECK_COMPILER_FLAG(${FLAG_LANG} ${NEW_FLAG} ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
	if(${flag_group})
	  set(${flag_group} "${${flag_group}} ${NEW_FLAG}")
	else(${flag_group})
	  set(${flag_group} "${NEW_FLAG}")
	endif(${flag_group})
      endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    endforeach(flag_group ${FLAG_GROUPS})

    # If a variable does not have a value, check the flag and if valid assign
    # the flag as the variable's value.  Do this for all supplied variables.
    foreach(flag_var ${FLAG_VARS})
      if(NOT ${flag_var})
	CHECK_COMPILER_FLAG(${FLAG_LANG} ${NEW_FLAG} ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
	if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND AND NOT "${${flag_var}}")
	  set(${flag_var} "${NEW_FLAG}")
	endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND AND NOT "${${flag_var}}")
      endif(NOT ${flag_var})
    endforeach(flag_var ${FLAG_VARS})
  endif(${ARGC} LESS 3)

endmacro(CHECK_FLAG)

# Convenience wrappers to call the primary checking function with a
# default language.
macro(CHECK_C_FLAG)
  CHECK_FLAG(C ${ARGN})
endmacro(CHECK_C_FLAG)
macro(CHECK_CXX_FLAG)
  CHECK_FLAG(CXX ${ARGN})
endmacro(CHECK_CXX_FLAG)

# Clear out most CMake-assigned defaults - We're managing
# our own compile flags, and don't (for example) want NDEBUG
# if we have debugging flags enabled for a Release build.
# At the same time, pull in any flags that have been set
# in the environment.

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_SHARED_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "")

if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
  set(CMAKE_C_FLAGS_${BUILD_TYPE_UPPER} "")
  set(CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER} "")
  set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE_UPPER} "")
  set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE_UPPER} "")
endif(CMAKE_BUILD_TYPE)

foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
  set(CMAKE_C_FLAGS_${CFG_TYPE_UPPER} "")
  set(CMAKE_CXX_FLAGS_${CFG_TYPE_UPPER} "")
  set(CMAKE_SHARED_LINKER_FLAGS_${CFG_TYPE_UPPER} "")
  set(CMAKE_EXE_LINKER_FLAGS_${CFG_TYPE_UPPER} "")
endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})

set(CMAKE_C_FLAGS "$ENV{CFLAGS}")
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "$ENV{LDFLAGS}")
#set(CMAKE_EXE_LINKER_FLAGS "")

# try to use -pipe to speed up the compiles
CHECK_C_FLAG(pipe)
CHECK_CXX_FLAG(pipe)

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
# linking against openNURBS in librt for some binaries, for
# example rttherm (i.e. any -static binaries)
CHECK_C_FLAG(fexceptions)
CHECK_CXX_FLAG(fexceptions)

# check for -ftemplate-depth-NN this is needed in libpc and
# other code using boost where the template instantiation depth
# needs to be increased from the default ANSI minimum of 17.
CHECK_CXX_FLAG(ftemplate-depth-128)

# dynamic SSE optimizations for NURBS processing
#
# XXX disable the SSE flags for now as they can cause illegal instructions.
#     the test needs to also be tied to run-time functionality since gcc
#     may still output SSE instructions (e.g., for cross-compiling).
# CHECK_C_FLAG(msse)
# CHECK_C_FLAG(msse2)
CHECK_C_FLAG(msse3 BUILD_TYPES Debug)

# Check for c90 support with gnu extensions if we're not building for
# a release so we get more broad portability testing.  Since the
# default is debug, it will be the more difficult to keep working
# given it's the lesser feature-rich C standard.  If we're going for
# a strictly standard compliant build, use the c** options instead
# of the gnu variations
if(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE)
  CHECK_C_FLAG("std=c89" BUILD_TYPES Debug)
  CHECK_C_FLAG("std=c99" BUILD_TYPES Release VARS C99_FLAG)
else()
  CHECK_C_FLAG("std=gnu89" BUILD_TYPES Debug)
  CHECK_C_FLAG("std=gnu99" BUILD_TYPES Release VARS C99_FLAG)
endif()

# What POSIX do we want to target, initially?  See
# http://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
# for options...
if(ENABLE_POSIX_COMPLIANCE)
  CHECK_C_FLAG("D_POSIX_C_SOURCE=200112L")
endif()

# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care.
CHECK_C_FLAG(Qunused-arguments)
CHECK_CXX_FLAG(Qunused-arguments)

# 64bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "64BIT")
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
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT")

# 32 bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO")
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
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO")

if(BRLCAD_ENABLE_PROFILING)
  CHECK_C_FLAG(pg VARS PROFILE_FLAG)
  CHECK_C_FLAG(p VARS PROFILE_FLAG)
  CHECK_C_FLAG(prof_gen VARS PROFILE_FLAG)
  if(NOT PROFILE_FLAG)
    message("Warning - profiling requested, but don't know how to profile with this compiler - disabling.")
    set(BRLCAD_ENABLE_PROFILING OFF)
  else(NOT PROFILE_FLAG)
    ADD_NEW_FLAG(C PROFILE_FLAG ALL)
    ADD_NEW_FLAG(CXX PROFILE_FLAG ALL)
  endif(NOT PROFILE_FLAG)
endif(BRLCAD_ENABLE_PROFILING)

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


# Set the minimum compilation linkage for Mac systems if not already
# set, no harm if set elsewhere.
if(APPLE)
  set(MACOSX_DEPLOYMENT_TARGET "$ENV{MACOSX_DEPLOYMENT_TARGET}")
  if(NOT MACOSX_DEPLOYMENT_TARGET)
    set(ENV(MACOSX_DEPLOYMENT_TARGET) "10.3")
  endif(NOT MACOSX_DEPLOYMENT_TARGET)

  if(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.3")
    CHECK_C_FLAG("mmacosx-version-min=10.3 -isysroot /Developer/SDKs/MacOSX10.3.9.sdk" VARS SDK_FLAG)
  endif(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.3")
  if(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.4")
    CHECK_C_FLAG("mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk" VARS SDK_FLAG)
  endif(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.4")
  if(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.5")
    CHECK_C_FLAG("mmacosx-version-min=10.5 -isysroot /Developer/SDKs/MacOSX10.5.sdk" VARS SDK_FLAG)
  endif(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.5")
  if(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.6")
    CHECK_C_FLAG("mmacosx-version-min=10.6 -isysroot /Developer/SDKs/MacOSX10.6.sdk" VARS SDK_FLAG)
  endif(MACOSX_DEPLOYMENT_TARGET STREQUAL "10.6")

  ADD_NEW_FLAG(C SDK_FLAG ALL)
  ADD_NEW_FLAG(CXX SDK_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER SDK_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER SDK_FLAG ALL)
endif(APPLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
