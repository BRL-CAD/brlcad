#             C O M P I L E R F L A G S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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

# Synopsis:  BRLCAD_CHECK_C_FLAG(flag [CONFIGURATIONS] [GATHERING VARIABLES] [UNIQUE VAL VARIABLES])
#
# Configurations refers to the C_FLAGS variable associated with build configurations such
# as CMAKE_C_FLAGS_DEBUG.  Gathering variables are variables intended to hold more than one
# compilation flag.  Unique value variables are intended to hold exactly one flag, and will
# not accept overwritting by this macro once a value has been assigned.
#
# For example, to test a flag and add it to the Debug configuration:
#
# BRLCAD_CHECK_C_FLAG(ggdb3 Debug)
#
# To assign a flag to a unique variable:
#
# BRLCAD_CHECK_C_FLAG(c99 "" "" C99_FLAG)
#
# To do all assignments at once, for multiple configs and vars:
#
# BRLCAD_CHECK_C_FLAG(ggdb3 "Debug;Release" "DEBUG_FLAGS" "DEBUG1;DEBUG2")
#
# For configurations, the keyword ALL is used for a flag that is
# supposed to be present regardless of configuration - this is
# the default.  After all flags are assigned, CMAKE_C_FLAGS (which
# collects the general C flags) is prepended to all other active
# C flag strings.
macro(BRLCAD_CHECK_C_FLAG flag)
  string(TOUPPER ${flag} UPPER_FLAG)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" UPPER_FLAG ${UPPER_FLAG})
  CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG_FOUND)
  if(${UPPER_FLAG}_COMPILER_FLAG_FOUND)
    set(NEW_FLAG "-${flag}")
    if(${ARGC} LESS 2)
      ADD_NEW_FLAG(C NEW_FLAG ALL)
    else(${ARGC} LESS 2)
      if(NOT "${ARGV1}" STREQUAL "")
        ADD_NEW_FLAG(C NEW_FLAG "${ARGV1}")
      endif(NOT "${ARGV1}" STREQUAL "")
      if(NOT "${ARGV2}" STREQUAL "")
	foreach(flagvar ${ARGV2})
	  if(${flagvar})
	    set(${flagvar} "${flagvar} -${flag}")
	  else(${flagvar})
	    set(${flagvar} "-${flag}")
	  endif(${flagvar})
	endforeach(flagvar ${ARGV2})
      endif(NOT "${ARGV2}" STREQUAL "")
      if(NOT "${ARGV3}" STREQUAL "")
	  foreach(flagvar ${ARGV3})
	    if(NOT ${flagvar})
	      set(${flagvar} "-${flag}")
	    endif(NOT ${flagvar})
	  endforeach(flagvar ${ARGV3})
      endif(NOT "${ARGV3}" STREQUAL "")
    endif(${ARGC} LESS 2)
  endif(${UPPER_FLAG}_COMPILER_FLAG_FOUND)
endmacro()

macro(BRLCAD_CHECK_CXX_FLAG flag)
  string(TOUPPER ${flag} UPPER_FLAG)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" UPPER_FLAG ${UPPER_FLAG})
  CHECK_CXX_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG_FOUND)
  if(${UPPER_FLAG}_COMPILER_FLAG_FOUND)
    set(NEW_FLAG "-${flag}")
    if(${ARGC} LESS 2)
      ADD_NEW_FLAG(CXX NEW_FLAG ALL)
    else(${ARGC} LESS 2)
      if(NOT "${ARGV1}" STREQUAL "")
        ADD_NEW_FLAG(CXX NEW_FLAG "${ARGV1}")
      endif(NOT "${ARGV1}" STREQUAL "")
      if(NOT "${ARGV2}" STREQUAL "")
	foreach(flagvar ${ARGV2})
	  if(${flagvar})
	    set(${flagvar} "${flagvar} -${flag}")
	  else(${flagvar})
	    set(${flagvar} "-${flag}")
	  endif(${flagvar})
	endforeach(flagvar ${ARGV2})
      endif(NOT "${ARGV2}" STREQUAL "")
      if(NOT "${ARGV3}" STREQUAL "")
	  foreach(flagvar ${ARGV3})
	    if(NOT ${flagvar})
	      set(${flagvar} "-${flag}")
	    endif(NOT ${flagvar})
	  endforeach(flagvar ${ARGV3})
      endif(NOT "${ARGV3}" STREQUAL "")
    endif(${ARGC} LESS 2)
  endif(${UPPER_FLAG}_COMPILER_FLAG_FOUND)
endmacro()

# Clear out any CMake-assigned defaults - We're managing
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

string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
set(CMAKE_C_FLAGS_${BUILD_TYPE} "$ENV{CFLAGS}")
set(CMAKE_CXX_FLAGS_${BUILD_TYPE} "$ENV{CXXFLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_${BUILD_TYPE} "$ENV{LDFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE} "")

# try to use -pipe to speed up the compiles
BRLCAD_CHECK_C_FLAG(pipe)
BRLCAD_CHECK_CXX_FLAG(pipe)

# check for -fno-strict-aliasing
# XXX - THIS FLAG IS REQUIRED if any level of optimization is
# enabled with GCC as we do use aliasing and type-punning.
BRLCAD_CHECK_C_FLAG(fno-strict-aliasing)
BRLCAD_CHECK_CXX_FLAG(fno-strict-aliasing)

# check for -fno-common (libtcl needs it on darwin)
BRLCAD_CHECK_C_FLAG(fno-common)
BRLCAD_CHECK_CXX_FLAG(fno-common)

# check for -fexceptions
# this is needed to resolve __Unwind_Resume when compiling and
# linking against openNURBS in librt for some binaries, for
# example rttherm (i.e. any -static binaries)
BRLCAD_CHECK_C_FLAG(fexceptions)
BRLCAD_CHECK_CXX_FLAG(fexceptions)

# check for -ftemplate-depth-NN this is needed in libpc and
# other code using boost where the template instantiation depth
# needs to be increased from the default ANSI minimum of 17.
BRLCAD_CHECK_CXX_FLAG(ftemplate-depth-128)

# dynamic SSE optimizations for NURBS processing
#
# XXX disable the SSE flags for now as they can cause illegal instructions.
#     the test needs to also be tied to run-time functionality since gcc
#     may still output SSE instructions (e.g., for cross-compiling).
# BRLCAD_CHECK_C_FLAG(msse)
# BRLCAD_CHECK_C_FLAG(msse2)
BRLCAD_CHECK_C_FLAG(msse3 Debug)

# Check for c90 support with gnu extensions if we're not building for a 
# release and c99 support with gnu extensions when we are building for a
# release just so we get broader portability testing - default development
# mode is Debug, so the default behavior will be to keep things working
# with the less feature-rich C standard.
#
# Also check for c99 conformance regardles since some platform
# environments require it due to c99-specific system headers (e.g.,
# /System/Library/Frameworks/OpenGL.framework/Headers/gl.h on Mac OS X
# having '//' comments embedded).  Do as the Autotools system did
# and use gnu99 here - c99 has problems on Linux.
# BRLCAD_CHECK_C_FLAG("std=gnu1x")
BRLCAD_CHECK_C_FLAG("std=gnu99" Release "" C99_FLAG)
BRLCAD_CHECK_C_FLAG("std=gnu89" Debug)

# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care. 
BRLCAD_CHECK_C_FLAG(Qunused-arguments)
BRLCAD_CHECK_CXX_FLAG(Qunused-arguments)

# 64bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "64BIT")
  BRLCAD_CHECK_C_FLAG(m64 "" "" 64BIT_FLAG)
  BRLCAD_CHECK_C_FLAG("arch x86_64" "" "" 64BIT_FLAG)
  BRLCAD_CHECK_C_FLAG(64 "" "" 64BIT_FLAG)
  BRLCAD_CHECK_C_FLAG("mabi=64" "" ""  64BIT_FLAG)
  if(NOT 64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
    message(FATAL_ERROR "Trying to compile 64BIT but all 64 bit compiler flag tests failed!")
  endif(NOT 64BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "64BIT")
  BRLCAD_CHECK_C_FLAG(q64 "" "" 64BIT_FLAG)
  ADD_NEW_FLAG(C 64BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 64BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 64BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 64BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT")

# 32 bit compilation flags
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  BRLCAD_CHECK_C_FLAG(m32 "" "" 32BIT_FLAG)
  BRLCAD_CHECK_C_FLAG("arch i686" "" "" 32BIT_FLAG)
  BRLCAD_CHECK_C_FLAG(32 "" "" 32BIT_FLAG)
  BRLCAD_CHECK_C_FLAG("mabi=32" "" "" 32BIT_FLAG)
  BRLCAD_CHECK_C_FLAG(q32 32BIT_FLAG)
  if(NOT 32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
    message(FATAL_ERROR "Trying to compile 32BIT but all 32 bit compiler flag tests failed!")
  endif(NOT 32BIT_FLAG AND ${CMAKE_WORD_SIZE} MATCHES "32BIT")
  ADD_NEW_FLAG(C 32BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 32BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 32BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 32BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT ${BRLCAD_WORD_SIZE} MATCHES "AUTO")

if(BRLCAD_ENABLE_PROFILING)
  BRLCAD_CHECK_C_FLAG(pg "" "" PROFILE_FLAG)
  BRLCAD_CHECK_C_FLAG(p "" "" PROFILE_FLAG)
  BRLCAD_CHECK_C_FLAG(prof_gen "" "" PROFILE_FLAG)
  if(NOT PROFILE_FLAG)
    message("Warning - profiling requested, but don't know how to profile with this compiler - disabling.")
    set(BRLCAD_ENABLE_PROFILING OFF)
  else(NOT PROFILE_FLAG)
    ADD_NEW_FLAG(C PROFILE_FLAG ALL)
    ADD_NEW_FLAG(CXX PROFILE_FLAG ALL)
  endif(NOT PROFILE_FLAG)
endif(BRLCAD_ENABLE_PROFILING)

# Debugging flags
if(${BRLCAD_DEBUG_BUILD} MATCHES "ON")
  if(APPLE)
    if(NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
      EXEC_PROGRAM(sw_vers ARGS -productVersion OUTPUT_VARIABLE MACOSX_VERSION)
      if(${MACOSX_VERSION} VERSION_LESS "10.5")
	BRLCAD_CHECK_C_FLAG(ggdb3 "" "" DEBUG_FLAG)
      else(${MACOSX_VERSION} VERSION_LESS "10.5")
	#CHECK_C_COMPILER_FLAG silently eats gstabs+
	set(DEBUG_FLAG "-gstabs+")
      endif(${MACOSX_VERSION} VERSION_LESS "10.5")
    endif(NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
  else(APPLE)
    BRLCAD_CHECK_C_FLAG(ggdb3 "" "" DEBUG_FLAG)
  endif(APPLE)
  BRLCAD_CHECK_C_FLAG(debug "" "" DEBUG_FLAG)
  if(${CMAKE_BUILD_TYPE} MATCHES "Release" AND BRLCAD_DEBUG_BUILD)
    set(debug_config_list "Debug;RelWithDebInfo")
  else(${CMAKE_BUILD_TYPE} MATCHES "Release" AND BRLCAD_DEBUG_BUILD)
    set(debug_config_list "Debug;RelWithDebInfo;Release")
  endif(${CMAKE_BUILD_TYPE} MATCHES "Release" AND BRLCAD_DEBUG_BUILD)
  ADD_NEW_FLAG(C DEBUG_FLAG "${debug_config_list}")
  ADD_NEW_FLAG(CXX DEBUG_FLAG "${debug_config_list}")
  ADD_NEW_FLAG(SHARED_LINKER DEBUG_FLAG "${debug_config_list}")
  ADD_NEW_FLAG(EXE_LINKER DEBUG_FLAG "${debug_config_list}")
  mark_as_advanced(DEBUG_FLAG)
endif(${BRLCAD_DEBUG_BUILD} MATCHES "ON")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
