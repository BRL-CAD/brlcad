#        B R L C A D _ B U I L D _ T Y P E S . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2024 United States Government as represented by
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
# Currently, I'm leaning towards #2 - forcing CMAKE_CONFIGURATION_TYPES to be
# set to a single value, matching a value supplied to CMAKE_BUILD_TYPE (or
# defaulting to Debug if no such setting is supplied.)  Essentially, this would
# force multiconfig build tools to work more like a basic make/ninja build,
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
# CMake by default provides four different configurations for multi-
# configuration build tools.  We want only two - Debug and Release.
if(NOT ENABLE_ALL_CONFIG_TYPES)
  if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
  endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
endif(NOT ENABLE_ALL_CONFIG_TYPES)

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

# TODO - in principle, this mechanism should be replaced with generator
# expressions
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



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
