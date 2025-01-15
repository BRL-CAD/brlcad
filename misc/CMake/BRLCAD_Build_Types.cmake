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

# Multiconfig on Windows with bext is a major problem.  Using Debug and Release
# DLLs together is hard:
# https://stackoverflow.com/questions/74857726/using-a-release-configured-dll-in-another-project-that-is-configured-for-debuggi
# and early experiments indicate BRL-CAD (and/or its dependencies) are not
# able to work mixing a Release bext with a Debug BRL-CAD build or vice versa.
#
# The options are basically:
# 1.  Have the CMake logic manage both debug and release configurations of
# bext, and try to handle only including and copying the configuration we need
# at runtime somehow.  Slow, complex, error prone, crazy high space
# requirements, hard to maintain...
#
# 2.  Disable multiconfig altogether - reduce the available build
# configurations displayed in Visual Studio to one, based on a CMake setting.
# We already reduce the types to Release and Debug to make Visual Studio match
# our other build configs, so the mechanism to do this does already exist.  The
# below logic can then build the one correct bext output type, and bundling
# becomes simple.  When an external bext_output is used, it will be the user's
# responsibility to make sure the supplied bext is of a type compatible with
# their selected build configuration.
#
# At the moment we're opting for #2 - forcing CMAKE_CONFIGURATION_TYPES to be
# set to a single value and matching a value supplied to CMAKE_BUILD_TYPE (or
# defaulting to Debug if no such setting is supplied.)  Essentially, this
# forces multiconfig build tools to work more like a basic make/ninja build,
# requiring a full configure pass to change build configs.  Users would be
# responsible for pointing the build to a bext_output with the correct build
# config for their proposed build type, if they're not going to have configure
# set things up for them.
#
# The constraints we're faced with when it comes to bext are:
#
# 1.  We need the build outputs of bext in place during the configure process
#     for find_package to detect - that would mean always building *both*
#     configurations if we have a multiconfig build tool.
# 2.  The bext contents must be copied to the correct runtime locations during
#     the configure process to stage them for potential use when executables
#     are run during the build process itself - adding configuration awareness
#     to that process would significantly increase the complexity.
# 3.  It's not clear how to get CMake's package detection to support finding
#     multiple configuration copies of library outputs - some packages have
#     at least some support for detecting multiple configs, but many do not.
# 4.  We would need configuration aware install rules for all the bext output.
# 5.  For the case where we don't have a pre-existing bext_output available
#     and BRL-CAD's configure process needs to do the full setup, we don't
#     want to have to compile all possible Windows build configurations of
#     bext during configure time.  In terms of time, space, and management
#     complexity that's asking for trouble.  A Debug build of BRL-CAD would
#     not want to waste time building both Debug and Release versions of bext
#     to have available, and we can't build bext at run time because the
#     BRL-CAD configure needs the build outputs of the bext compile to work.
#
# Bottom line - because a huge amount of significant work now has to be
# finished *before* the BRL-CAD configure is complete, we can't afford to defer
# the build type decision to build time anymore.

#---------------------------------------------------------------------
# Normalize the build type capitalization
if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
  if("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
  endif("${BUILD_TYPE_UPPER}" STREQUAL "RELEASE")
  if("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
  endif("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG")
endif(CMAKE_BUILD_TYPE)


# If we don't have any build type defined at all, go with Release.  Thinking is
# that for non-debugging use this will provide users with the best performance.
# We build with debug info enabled by default, even in Release, so the main
# advantages of Debug building are better backtraces/debugging behavior and
# shorter build times.  Those aren't critical for normal use, so make the
# "default" setting the one that will give new users what they most likely are
# expecting/wanting.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

# CMake configuration types need to be overridden.  If a CMAKE_BUILD_TYPE
# has been specified, use that - otherwise default to Debug.
if(CMAKE_CONFIGURATION_TYPES)
  if(CMAKE_BUILD_TYPE)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}" CACHE STRING "Force a single build type" FORCE)
  else(CMAKE_BUILD_TYPE)
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "Force a single build type" FORCE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
  endif(CMAKE_BUILD_TYPE)
endif(CMAKE_CONFIGURATION_TYPES)

mark_as_advanced(CMAKE_BUILD_TYPE)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
