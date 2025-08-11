#        B R L C A D _ B U I L D _ T Y P E S . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2025 United States Government as represented by
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
#
# CMAKE_CONFIGURATION_TYPES is forced to a single value that matches
# the supplied CMAKE_BUILD_TYPE (or defaulting to Release if no such
# setting is supplied).  This is done because bext compilation must be
# finished *before* the BRL-CAD configure is complete, thus we can't
# afford to defer the build type decision to build time anymore.
#
# We disable multiconfig altogether (elsewhere) which reduces the
# available build configurations displayed in Visual Studio, XCode,
# and similar to just the CMAKE_BUILD_TYPE setting.  Supported build
# types are Release or Debug (not currently imposed here).
#
# NOTE: When an external bext_output is used, it is the user's
# responsibility to make sure the supplied bext is compatible with
# their selected build configuration.
#
###

#---------------------------------------------------------------------
# Function to initialize Release as the default build type.
function(InitializeBuildType)
  # Release will provide users with the best performance.  Debugging
  # should be already enabled by default (elsewhere), even for
  # Release, for backtraces and debuggability.
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
  endif()

  # CMake configuration types need override.  If CMAKE_BUILD_TYPE is
  # specified, use that - otherwise default to Release.
  if(CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_BUILD_TYPE)
      set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}" CACHE STRING "Force a single build type" FORCE)
    else()
      set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Force a single build type" FORCE)
      set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
    endif()
  endif()

  # Mark variables as advanced
  mark_as_advanced(CMAKE_BUILD_TYPE)
  mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
endfunction()


#---------------------------------------------------------------------
# Function to normalize build type capitalization.
function(NormalizeBuildType)
  if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
    if("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
      set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
    elseif("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
      set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
    endif()
  endif()

  # Mark CMAKE_BUILD_TYPE as advanced since it is modified
  mark_as_advanced(CMAKE_BUILD_TYPE)
endfunction()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
