#  B R L C A D _ E N V I R O N M E N T _ S E T U P . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2022 United States Government as represented by
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
# Setup and checks related to system environment settings

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
# Characterize the system as 32 or 64 bit - this has an impact on many
# of the subsequent operations, including find_package results, so it
# must be done up front.
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
mark_as_advanced(BRLCAD_WORD_SIZE)

# calculate the size of a pointer if we haven't already
include(CheckTypeSize)
check_type_size("void *" CMAKE_SIZEOF_VOID_P)

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

# If a platform specific variable needs to be set for 32 bit, do it here
if (${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set(CMAKE_OSX_ARCHITECTURES "i386" CACHE STRING "Building for i386" FORCE)
endif (${CMAKE_WORD_SIZE} MATCHES "32BIT")

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


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
