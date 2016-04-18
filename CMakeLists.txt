#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2016 United States Government as represented by
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


# We want to support a "distclean" build target that will clear all
# CMake-generated files from a source directory in the case of an
# in-source-dir configuration.  Not recommended, but we'll try to
# recover if it happens.
include(${CMAKE_SOURCE_DIR}/misc/CMake/distclean.cmake)

# Stash the initial CMakeCache.txt file, if it exists, for subsequent
# comparison
if(EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt")
  configure_file("${CMAKE_BINARY_DIR}/CMakeCache.txt" "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev" COPYONLY)
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
endif(EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt")
DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeCache.txt")

# We want to capture and save the command line arguments
include(${CMAKE_SOURCE_DIR}/misc/CMake/command_line_args.cmake)

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.8)
if(COMMAND CMAKE_POLICY)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
    CMAKE_POLICY(SET CMP0042 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)

  if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
    CMAKE_POLICY(SET CMP0046 NEW)
    CMAKE_POLICY(SET CMP0053 NEW)
    CMAKE_POLICY(SET CMP0054 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)

# set CMake project name - default to BRL-CAD, but if
# a subproject is present use that instead.  The subproject
# cmake file needs to define the following:
#
# SUBPROJECT - name of subproject
# SUBPROJ_VERSION - version of subproject
if(EXISTS ${CMAKE_SOURCE_DIR}/subproject.cmake)
  include("${CMAKE_SOURCE_DIR}/subproject.cmake" SUBPROJECT)
  string(STRIP ${SUBPROJECT} SUBPROJECT)
  project(${SUBPROJECT})
  set(PROJ_VERSION ${SUBPROJ_VERSION})
else(EXISTS ${CMAKE_SOURCE_DIR}/subproject.cmake)
  project(BRLCAD)
endif(EXISTS ${CMAKE_SOURCE_DIR}/subproject.cmake)

# Many of the CMake bits we want/need are specific to BRL-CAD -
# test the project name and include them only when we need to
macro(BINCLUDE cfile)
  if("${CMAKE_PROJECT_NAME}" STREQUAL "BRLCAD")
    include("${cfile}")
  endif("${CMAKE_PROJECT_NAME}" STREQUAL "BRLCAD")
endmacro(BINCLUDE cfile)

# Wrap the default message() function (must come after calling project)
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/message_override.cmake")

# Init timing delta info
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/timedelta_start.cmake")

# Check environment for BRL-CAD overrides
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_env_init.cmake")

# Load general utility routines for BRL-CAD CMake logic
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Util.cmake")

# Define CMake routines for bookkeeping of build files
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_CMakeFiles.cmake")

# Make sure the project library list is initialized empty - this way
# if pieces are disabled between one configure and the next, they
# won't linger in the list
set(PROJECT_LIBS "" CACHE STRING "Project libraries" FORCE)

# If project is BRL-CAD get the version
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Version.cmake")

# Add misc/CMake to the CMake modules path.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/misc/CMake" ${CMAKE_MODULE_PATH})

# Define relative install locations and output directories.
include("${CMAKE_SOURCE_DIR}/misc/CMake/Path_Setup.cmake")

# Don't search in install path
include("${CMAKE_SOURCE_DIR}/misc/CMake/disable_install_path_search.cmake")

# Define check and unit test targets
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/test_targets.cmake")

# BRL-CAD distcheck book keeping function overrides (needs check and unit
# targets defined first
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/command_overrides.cmake")

#---------------------------------------------------------------------
# Load macros that will be used to define the BRL-CAD
# build logic
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Options.cmake")
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Targets.cmake")

# Check and set lang environment variable
include("${CMAKE_SOURCE_DIR}/misc/CMake/lang_env.cmake")

# Valid package creation with CMake depends on the value of umask
include("${CMAKE_SOURCE_DIR}/misc/CMake/umask_check.cmake")

# Get timestamp
include("${CMAKE_SOURCE_DIR}/misc/CMake/get_timestamp.cmake")

# print out the title with a pretty box computed to wrap around
BOX_PRINT("*** Configuring BRL-CAD Release ${PROJ_VERSION}, Build ${CONFIG_DATE} ***" "*")

# CMake by default provides four different configurations for multi-
# configuration build tools.  We want only two - Debug and Release.
include("${CMAKE_SOURCE_DIR}/misc/CMake/normalize_build_types.cmake")

# We want to check /usr/local by default, so add it
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)

# The location in which to install BRL-CAD, if it is BRL-CAD we are
# building.
BINCLUDE("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Install_Prefix.cmake")

# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling
include("${CMAKE_SOURCE_DIR}/misc/CMake/RPath_Setup.cmake")

# The brlcad_config.h header is used universally.
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Config_Setup.cmake")

# Decide whether to do a 32 or a 64 bit build.
include("${CMAKE_SOURCE_DIR}/misc/CMake/word_size.cmake")

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
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*        Stage 1 of 9 - Top Level Configure Options       *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
#
# Now we define the various options for BRL-CAD - ways to enable and
# disable features, select which parts of the system to build, etc.
# As much as possible, sane default options are either selected or
# detected.  Because documentation is autogenerated for BRL-CAD
# options, be sure to initialize the file.
set(CONFIG_OPT_STRING "CONFIGURATION OPTIONS\n---------------------\n")
file(WRITE "${CMAKE_BINARY_DIR}/OPTIONS" "${CONFIG_OPT_STRING}")

# The BRL-CAD CMake build will also generate a configure script
# that emulates the command option style of GNU Autotool's
# configure.  Write the pre-defined header into the build-dir template
# to initialize the file.
file(REMOVE "${CMAKE_BINARY_DIR}/configure.new")
file(READ "${CMAKE_SOURCE_DIR}/misc/CMake/configure_prefix.sh" CONFIG_PREFIX)
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
if(NOT CMAKE_CONFIGURATION_TYPES)
  AUTO_OPTION(BRLCAD_BUILD_STATIC_LIBS BUILD_STATIC_LIBS "OFF" "ON")
else(NOT CMAKE_CONFIGURATION_TYPES)
  if("${BRLCAD_BUILD_STATIC_LIBS}" STREQUAL "")
    set(BRLCAD_BUILD_STATIC_LIBS ON)
  endif("${BRLCAD_BUILD_STATIC_LIBS}" STREQUAL "")
  set(BUILD_STATIC_LIBS "${BRLCAD_BUILD_STATIC_LIBS}")
endif(NOT CMAKE_CONFIGURATION_TYPES)

# On Mac OS X, it is common to have third party package managers
# present for easy software installation (currently we're aware of
# Fink and MacPorts).  This can seriously complicate find_* results,
# so provide an option to specify whether or which of the third
# party setup to use.
include("${CMAKE_SOURCE_DIR}/misc/CMake/Fink_MacPorts.cmake")

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

if(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "BUNDLED")
else(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "AUTO")
endif(MSVC)
BRLCAD_OPTION(${BRLCAD_BUNDLED_LIBS_DEFAULT} BRLCAD_BUNDLED_LIBS BRLCAD_BUNDLED_LIBS_ALIASES BRLCAD_BUNDLED_LIBS_DESCRIPTION)
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
  set(BRLCAD_BUNDLED_LIBS "AUTO" CACHE STRING "Build bundled libraries." FORCE)
endif(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")

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
if(NOT BRLCAD_ENABLE_AQUA)
  include("${CMAKE_SOURCE_DIR}/misc/CMake/FindX11.cmake")
endif(NOT BRLCAD_ENABLE_AQUA)

# Set whether X11 is enabled or disabled by default
if(WIN32)
  # even if there is x11, we default to native
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
elseif(BRLCAD_ENABLE_AQUA)
  # aqua implies no X11
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
else(WIN32)
  # make everywhere else depend on whether we found a suitable X11
  if(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
    option(BRLCAD_ENABLE_X11 "Use X11." ON)
  else(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
    option(BRLCAD_ENABLE_X11 "Use X11." OFF)
  endif(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
endif(WIN32)
mark_as_advanced(BRLCAD_ENABLE_X11)

# if X11 is enabled, make sure aqua is off
if(BRLCAD_ENABLE_X11)
  set(BRLCAD_ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
  set(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif(BRLCAD_ENABLE_X11)
mark_as_advanced(OPENGL_USE_AQUA)

# Enable/disable features requiring the Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tk
option(BRLCAD_ENABLE_TK "Enable features requiring the Tk toolkit" ON)
mark_as_advanced(BRLCAD_ENABLE_TK)
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
  include("${CMAKE_SOURCE_DIR}/misc/CMake/FindGL.cmake")
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)

set(BRLCAD_ENABLE_OPENGL_ALIASES
  ENABLE_OPENGL
  )
set(BRLCAD_ENABLE_OPENGL_DESCRIPTION "
Enable support for OpenGL based Display Managers in BRL-CAD.
Default depends on whether OpenGL is successfully detected -
if it is, default is to enable.
")
BRLCAD_OPTION(${OPENGL_FOUND} BRLCAD_ENABLE_OPENGL BRLCAD_ENABLE_OPENGL_ALIASES BRLCAD_ENABLE_OPENGL_DESCRIPTION)

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

# Enable features requiring Qt
find_package(Qt5Widgets QUIET)
option(BRLCAD_ENABLE_QT "Enable features requiring Qt" OFF)
mark_as_advanced(BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Core_DIR)
mark_as_advanced(Qt5Gui_DIR)
if(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
  message("QT interface requested, but QT5 is not found - disabling")
  set(BRLCAD_ENABLE_QT OFF)
endif(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Widgets_DIR)

# Enable features requiring OpenSceneGraph
option(BRLCAD_ENABLE_OSG "Enable features requiring OpenSceneGraph" OFF)
mark_as_advanced(BRLCAD_ENABLE_OSG)
if(BRLCAD_ENABLE_OSG)
  if(APPLE AND NOT BRLCAD_ENABLE_AQUA)
    set(OSG_WINDOWING_SYSTEM "X11" CACHE STRING "Use X11" FORCE)
  endif(APPLE AND NOT BRLCAD_ENABLE_AQUA)
endif(BRLCAD_ENABLE_OSG)

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

# Enable features requiring Screened Poisson Surface Reconstruction
option(BRLCAD_ENABLE_SPR "Enable features requiring Screened Poisson Surface Reconstruction" OFF)
mark_as_advanced(BRLCAD_ENABLE_SPR)

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
BRLCAD_OPTION(ON BRLCAD_ENABLE_RUNTIME_DEBUG BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION)
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
set(BRLCAD_FLAGS_DEBUG_ALIASES
  ENABLE_DEBUG
  ENABLE_FLAGS_DEBUG
  ENABLE_DEBUG_FLAGS
  )
set(BRLCAD_FLAGS_DEBUG_DESCRIPTION "
Add compiler flags to aid in program debugging.  Defaults to ON.
")
BRLCAD_OPTION(ON BRLCAD_FLAGS_DEBUG BRLCAD_FLAGS_DEBUG_ALIASES BRLCAD_FLAGS_DEBUG_DESCRIPTION)

# A variety of debugging messages in the code key off of the DEBUG
# definition - set it according to whether we're using debug flags.
if(BRLCAD_FLAGS_DEBUG)
  CONFIG_H_APPEND(BRLCAD "#define DEBUG 1\n")
endif(BRLCAD_FLAGS_DEBUG)

# Build with compiler warning flags
set(BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  ENABLE_WARNINGS
  ENABLE_COMPILER_WARNINGS
  )
set(BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION "
Use extra compiler warning flags when compiling C/C++ code.  Defaults to ON.
")
BRLCAD_OPTION(ON BRLCAD_ENABLE_COMPILER_WARNINGS
  BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_COMPILER_WARNINGS)

# Enable/disable strict compiler settings - these are used for building
# BRL-CAD by default, but not src/other code.  Always used for BRL-CAD
# code unless the NO_STRICT option is specified when defining a target
# with BRLCAD_ADDEXEC or BRLCAD_ADDLIB.  If only C++ files in a target
# are not compatible with strict, the NO_STRICT_CXX option can be used.
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
BRLCAD_OPTION(ON BRLCAD_ENABLE_STRICT BRLCAD_ENABLE_STRICT_ALIASES BRLCAD_ENABLE_STRICT_DESCRIPTION)
if(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_STRICT)
  CONFIG_H_APPEND(BRLCAD "#define STRICT_FLAGS 1\n")
endif(BRLCAD_ENABLE_STRICT)

# Build with compiler optimization flags.  This should normally be on for
# release builds.
AUTO_OPTION(BRLCAD_FLAGS_OPTIMIZATION BRLCAD_OPTIMIZED_BUILD "OFF" "ON")
if(CMAKE_CONFIGURATION_TYPES)
  mark_as_advanced(BRLCAD_FLAGS_OPTIMIZATION)
endif(CMAKE_CONFIGURATION_TYPES)

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

#====== ALL CXX COMPILE ===================
# Build all C and C++ files with a C++ compiler
set(ENABLE_ALL_CXX_COMPILE_ALIASES "ENABLE_ALL_CXX")
set(ENABLE_ALL_CXX_COMPILE_DESCRIPTION "
Build all C and C++ files with a C++ compiler.  Defaults to OFF.

EXPERIMENTAL!
")
BRLCAD_OPTION(OFF ENABLE_ALL_CXX_COMPILE
  ENABLE_ALL_CXX_COMPILE_ALIASES
  ENABLE_ALL_CXX_COMPILE_DESCRIPTION)
mark_as_advanced(ENABLE_ALL_CXX_COMPILE)

# Build with coverage enabled
option(BRLCAD_ENABLE_COVERAGE "Build with coverage enabled" OFF)
mark_as_advanced(BRLCAD_ENABLE_COVERAGE)

#====== POSIX ===================
# Build with strict POSIX compliance checking
set(ENABLE_POSIX_COMPLIANCE_ALIASES "ENABLE_POSIX")
set(ENABLE_POSIX_COMPLIANCE_DESCRIPTION "
Build with strict POSIX compliance checking.  Defaults to OFF.

Causes compiler options to be set for strict compliance with the
minimum C and C++ standards acceptable according to current BRL-CAD
policy.  The current minimum C standard is ??.
There is no currently defined minimum C++ standard.
")
BRLCAD_OPTION(OFF ENABLE_POSIX_COMPLIANCE
  ENABLE_POSIX_COMPLIANCE_ALIASES
  ENABLE_POSIX_COMPLIANCE_DESCRIPTION)
mark_as_advanced(ENABLE_POSIX_COMPLIANCE)

#== ISO C ==
set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_ALIASES "STRICT_ISO_C")
set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_DESCRIPTION "
Build with strict ISO C compliance checking.  Defaults to OFF.

Causes C compiler options to be set for strict compliance with the
appropriate ISO C standard.
")
BRLCAD_OPTION(OFF ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE
  ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_ALIASES
  ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_DESCRIPTION)
mark_as_advanced(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE)

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
set(BRLCAD_EXTRADOCS_ALIASES
  ENABLE_DOCS
  ENABLE_EXTRA_DOCS
  ENABLE_DOCBOOK
  )
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
BRLCAD_OPTION(${EXTRADOCS_DEFAULT} BRLCAD_EXTRADOCS BRLCAD_EXTRADOCS_ALIASES BRLCAD_EXTRADOCS_DESCRIPTION)

include(CMakeDependentOption)

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
include("${CMAKE_SOURCE_DIR}/misc/CMake/Doxygen.cmake")

#----------------------------------------------------------------------
# Load various wrapper macros for checking libraries, headers and
# functions, some in use by src/other build logic
include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_CheckFunctions.cmake")


# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*             Stage 2 of 9 - Check for Programs           *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

# A variety of tools, such as the benchmark utilities, need
# a Bourne shell and other commands - check for them.
include("${CMAKE_SOURCE_DIR}/misc/CMake/FindShellDeps.cmake")

# CMake can detect flex, but apparently not generic lex.
# Solution is to make our own generic lex Find routine that looks
# first for flex, but will take whatever's available.
include("${CMAKE_SOURCE_DIR}/misc/CMake/FindLEX.cmake")

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
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*     Stage 3 of 9 - Check for Compiler Characteristics   *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

# load our compiler testing macro definitions
include("${CMAKE_SOURCE_DIR}/misc/CMake/CompilerFlags.cmake")

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
  CHECK_C_FLAG("std=c99" BUILD_TYPES Debug)
  CHECK_C_FLAG("std=c99" BUILD_TYPES Release VARS C99_FLAG)
  # we need to define some things as "empty" to avoid false positives
  # for bu_* functions using varargs and print specifiers
  CHECK_C_FLAG("D_BU_ATTR_PRINTF12=''")
  CHECK_C_FLAG("D_BU_ATTR_PRINTF23=''")
  CHECK_C_FLAG("D_BU_ATTR_SCANF23=''")
  # a hack to temp work around signal() handling in interrupt.c
  CHECK_C_FLAG("DC99_POSIX_USE_BSD")
  #=== C++
  CHECK_CXX_FLAG("std=c++03" BUILD_TYPES Debug)
  CHECK_CXX_FLAG("std=c++03" BUILD_TYPES Release VARS CXX03_FLAG)
  # for bu_* functions using varargs and print specifiers
  CHECK_CXX_FLAG("D_BU_ATTR_PRINTF12=''")
  CHECK_CXX_FLAG("D_BU_ATTR_PRINTF23=''")
  CHECK_CXX_FLAG("D_BU_ATTR_SCANF23=''")
  # a hack to temp work around signal() handling in interrupt.c
  CHECK_CXX_FLAG("DC99_POSIX_USE_BSD")
else()
  CHECK_C_FLAG("std=gnu89" BUILD_TYPES Debug)
  CHECK_C_FLAG("std=gnu99" BUILD_TYPES Release VARS C99_FLAG)
  CHECK_CXX_FLAG("std=gnu++98" BUILD_TYPES Debug)
  CHECK_CXX_FLAG("std=gnu++98" BUILD_TYPES Release VARS CXX03_FLAG)
endif()
# TODO - should be using this with MSVC, but it breaks the BUILD_SLEEP
# try_compile below with errors that appear to be coming from Windows
# headers??
# CHECK_C_FLAG("Za" BUILD_TYPES ALL)

# What POSIX do we want to target, initially?  See
# http://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
# for options...
if(ENABLE_POSIX_COMPLIANCE)
  # use this version so three non-C99 functions (without bu*
  # replacements) will work (fileno, fdopen, and posix_memalign):
  CHECK_C_FLAG("D_POSIX_C_SOURCE=200112L")
  # need this for glibc < 2.12 (for gethostname):
  CHECK_C_FLAG("D_XOPEN_SOURCE=500")
  #=== same for C++
  CHECK_CXX_FLAG("D_POSIX_C_SOURCE=200112L")
  CHECK_CXX_FLAG("D_XOPEN_SOURCE=500")
endif()

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

# Most of the src/other projects have their own logic for handling
# the C inline issue - BRL-CAD needs a fine-grained approach.  Mixed
# C and C++ sources require different treatment for the same build
# target, since C++11 doesn't allow inline to be re-defined. See
# misc/CMake/BRLCAD_Targets.cmake for handling of C_INLINE.
CHECK_C_INLINE(C_INLINE)
if(NOT HAVE_INLINE_KEYWORD AND HAVE___INLINE_KEYWORD)
  CONFIG_H_APPEND(BRLCAD "#ifndef __cplusplus\n")
  CONFIG_H_APPEND(BRLCAD "#  define inline __inline\n")
  CONFIG_H_APPEND(BRLCAD "#endif /* not __cplusplus */\n")
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

set(CMAKE_REQUIRED_FLAGS_BAK ${CMAKE_REQUIRED_FLAGS})
CHECK_C_FLAG(Werror VARS ERROR_FLAG)
if (ERROR_FLAG)
  set(CMAKE_REQUIRED_FLAGS "-Werror")
endif (ERROR_FLAG)

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


# Silence check for unused arguments (used to silence clang warnings about
# unused options on the command line). By default clang generates a lot of
# warnings about such arguments, and we don't really care.
CHECK_C_FLAG(Qunused-arguments)
CHECK_CXX_FLAG(Qunused-arguments)


set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_BAK})

# ******************************************************************* #

if(NOT MSVC)
  # We could include this, but these tests are almost entirely for
  # gcc-style flags.  This file is intentionally included after we
  # wipe out the various FLAGS variables.
  include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_CompilerFlags.cmake")
endif(NOT MSVC)


# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*             Stage 4 of 9 - Check for Libraries          *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

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

# Check for the C++ STL library - need to link it explicitly in
# some compilation situations
find_package(STL)

# Check for the daemon function in -lbsd and/or -lc for adrt
BRLCAD_CHECK_LIBRARY(BSD bsd daemon)
BRLCAD_CHECK_LIBRARY(BSD c daemon)

# Check for CoreFoundation and Cocoa on Apple
if(APPLE)
  include(CMakeFindFrameworks)
  CMAKE_FIND_FRAMEWORKS(CoreFoundation)
  if(CoreFoundation_FRAMEWORKS)
    set(CoreFoundation_LIBRARIES "-framework CoreFoundation" CACHE FILEPATH "CoreFoundation framework" FORCE)
  endif(CoreFoundation_FRAMEWORKS)
  CMAKE_FIND_FRAMEWORKS(Cocoa)
  if(Cocoa_FRAMEWORKS)
    set(Cocoa_LIBRARIES "-framework Cocoa" CACHE FILEPATH "Cocoa framework" FORCE)
  endif(Cocoa_FRAMEWORKS)
  mark_as_advanced(CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
  mark_as_advanced(CoreFoundation_LIBRARIES Cocoa_LIBRARIES)
endif(APPLE)

# Mark X11 as available if it is enabled and we find Xlib.h
if(BRLCAD_ENABLE_X11)
  if(X11_Xlib_INCLUDE_PATH AND CONFIG_H_FILE)
    set(HAVE_X11_XLIB_H 1)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_X11_XLIB_H 1\n")
  endif(X11_Xlib_INCLUDE_PATH AND CONFIG_H_FILE)
endif(BRLCAD_ENABLE_X11)

# math library
BRLCAD_CHECK_LIBRARY(M m cos)

# uuid library
BRLCAD_CHECK_LIBRARY(UUID uuid uuid_generate)

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
  set(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "winmm.lib")
  check_c_source_compiles("#include <windows.h>\nint main() {(void)timeSetEvent(1000, 100, (LPTIMECALLBACK)NULL, (DWORD_PTR)NULL, TIME_ONESHOT);}" HAVE_TIMESETEVENT)
  if(HAVE_TIMESETEVENT)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TIMESETEVENT 1\n")
    set(WINMM_LIBRARY "winmm.lib")
  endif(HAVE_TIMESETEVENT)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
endif("${HAVE_TIMESETEVENT}" MATCHES "^${HAVE_TIMESETEVENT}$")


# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*               Stage 5 of 9 - Check for Headers          *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

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

# Need to compare this with the configure.ac list
BRLCAD_INCLUDE_FILE(arpa/inet.h HAVE_ARPA_INET_H)
BRLCAD_INCLUDE_FILE(curses.h HAVE_CURSES_H)
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
BRLCAD_INCLUDE_FILE(mach/thread_policy.h HAVE_MACH_THREAD_POLICY_H)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(ncurses.h HAVE_NCURSES_H)
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
BRLCAD_INCLUDE_FILE(uuid/uuid.h HAVE_UUID_UUID_H) # for uuid_generate() on Mac & Linux */
BRLCAD_INCLUDE_FILE(windows.h HAVE_WINDOWS_H) # for QueryPerformanceCounter() on Windows

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
  check_cxx_source_compiles("#include <emmintrin.h>\nint main() { return 0; }" HAVE_EMMINTRIN)
  if(HAVE_EMMINTRIN)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_EMMINTRIN 1\n")
  endif(HAVE_EMMINTRIN)
endif(HAVE_EMMINTRIN_H)

if(BRLCAD_ENABLE_SPR)
  CONFIG_H_APPEND(BRLCAD "#define ENABLE_SPR 1\n")
endif(BRLCAD_ENABLE_SPR)

# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*          Stage 6 of 9 - Check for Types/Structures      *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")


BRLCAD_STRUCT_MEMBER("struct stat" st_blksize sys/stat.h STRUCT_STAT_ST_BLKSIZE)
BRLCAD_STRUCT_MEMBER("struct stat" st_blocks sys/stat.h STRUCT_STAT_ST_BLOCKS)
BRLCAD_STRUCT_MEMBER("struct stat" st_rdev sys/stat.h STRUCT_STAT_ST_RDEV)

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

# see if the compiler supports %z as a size_t print width specifier
BRLCAD_CHECK_C99_FORMAT_SPECIFIERS()

# see if the compiler supports [static] array hints
BRLCAD_CHECK_STATIC_ARRAYS()


# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*              Stage 7 of 9 - Check for Functions         *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")


# ALLOCA test - based on AC_FUNC_ALLOCA
BRLCAD_ALLOCA()

BRLCAD_FUNCTION_EXISTS(XQueryExtension HAVE_XQUERYEXTENSION)
BRLCAD_FUNCTION_EXISTS(_putenv_s HAVE__PUTENV_S)
BRLCAD_FUNCTION_EXISTS(alarm HAVE_ALARM)
BRLCAD_FUNCTION_EXISTS(dlopen HAVE_DLOPEN)
BRLCAD_FUNCTION_EXISTS(drand48 HAVE_DRAND48)
BRLCAD_FUNCTION_EXISTS(fchmod HAVE_FCHMOD)
BRLCAD_FUNCTION_EXISTS(fsync HAVE_FSYNC)
BRLCAD_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
BRLCAD_FUNCTION_EXISTS(getegid HAVE_GETEGID)
BRLCAD_FUNCTION_EXISTS(getenv_s HAVE_GETENV_S)
BRLCAD_FUNCTION_EXISTS(geteuid HAVE_GETEUID)
BRLCAD_FUNCTION_EXISTS(gethostname HAVE_GETHOSTNAME)
BRLCAD_FUNCTION_EXISTS(getloadavg HAVE_GETLOADAVG)
BRLCAD_FUNCTION_EXISTS(getopt_long HAVE_GETOPT_LONG)
BRLCAD_FUNCTION_EXISTS(getprogname HAVE_GETPROGNAME) # bsd
BRLCAD_FUNCTION_EXISTS(htonll HAVE_HTONLL)
BRLCAD_FUNCTION_EXISTS(logb HAVE_LOGB REQUIRED_LIBS ${M_LIBRARY})
BRLCAD_FUNCTION_EXISTS(lrand48 HAVE_LRAND48)
BRLCAD_FUNCTION_EXISTS(mkstemp HAVE_MKSTEMP)
BRLCAD_FUNCTION_EXISTS(ntohll HAVE_NTOHLL)
BRLCAD_FUNCTION_EXISTS(pipe HAVE_PIPE)
BRLCAD_FUNCTION_EXISTS(popen HAVE_POPEN)
BRLCAD_FUNCTION_EXISTS(posix_memalign HAVE_POSIX_MEMALIGN) # IEEE Std 1003.1-2001
BRLCAD_FUNCTION_EXISTS(random HAVE_RANDOM)
BRLCAD_FUNCTION_EXISTS(realpath HAVE_REALPATH)
BRLCAD_FUNCTION_EXISTS(setenv HAVE_SETENV)
BRLCAD_FUNCTION_EXISTS(setpgid HAVE_SETPGID)
BRLCAD_FUNCTION_EXISTS(setpriority HAVE_SETPRIORITY)
BRLCAD_FUNCTION_EXISTS(shmat HAVE_SHMAT)
BRLCAD_FUNCTION_EXISTS(shmctl HAVE_SHMCTL)
BRLCAD_FUNCTION_EXISTS(shmdt HAVE_SHMDT)
BRLCAD_FUNCTION_EXISTS(shmget HAVE_SHMGET)
BRLCAD_FUNCTION_EXISTS(srand48 HAVE_SRAND48)
BRLCAD_FUNCTION_EXISTS(strcasecmp HAVE_STRCASECMP)
BRLCAD_FUNCTION_EXISTS(strdup HAVE_STRDUP)
BRLCAD_FUNCTION_EXISTS(strlcat HAVE_STRLCAT)
BRLCAD_FUNCTION_EXISTS(strlcpy HAVE_STRLCPY)
BRLCAD_FUNCTION_EXISTS(strncasecmp HAVE_STRNCASECMP)
BRLCAD_FUNCTION_EXISTS(sync HAVE_SYNC)
BRLCAD_FUNCTION_EXISTS(sysconf HAVE_SYSCONF)
BRLCAD_FUNCTION_EXISTS(sysctl HAVE_SYSCTL)
BRLCAD_FUNCTION_EXISTS(sysmp HAVE_SYSMP)
BRLCAD_FUNCTION_EXISTS(vfork HAVE_VFORK)
BRLCAD_FUNCTION_EXISTS(vsscanf HAVE_VSSCANF)
BRLCAD_FUNCTION_EXISTS(writev HAVE_WRITEV)

# test for daemon(), which is deprecated on Mac OS X 10.5+
BRLCAD_FUNCTION_EXISTS(daemon HAVE_DAEMON)
if (HAVE_DAEMON)
  check_c_source_compiles("#include <stdlib.h>\nint main() { (void)daemon; return 0; }" HAVE_WORKING_DAEMON_FUNCTION)
endif (HAVE_DAEMON)

# ntohll and htonll maybe harder to spot - do some extra tests
if("${HAVE_NTOHLL}" STREQUAL "")
  CHECK_SYMBOL_EXISTS(ntohll "sys/_endian.h" HAVE_NTOHLL_SYS__ENDIAN)
  if(HAVE_NTOHLL_SYS__ENDIAN AND CONFIG_H_FILE)
    set(HAVE_NTOHLL 1 CACHE INTERNAL "Have function NTOHLL")
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_NTOHLL 1\n")
  endif(HAVE_NTOHLL_SYS__ENDIAN AND CONFIG_H_FILE)
endif("${HAVE_NTOHLL}" STREQUAL "")
if("${HAVE_HTONLL}" STREQUAL "")
  CHECK_SYMBOL_EXISTS(htonll "sys/_endian.h" HAVE_HTONLL_SYS__ENDIAN)
  if(HAVE_HTONLL_SYS__ENDIAN AND CONFIG_H_FILE)
    set(HAVE_HTONLL 1 CACHE INTERNAL "Have function HTONLL")
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_HTONLL 1\n")
  endif(HAVE_HTONLL_SYS__ENDIAN AND CONFIG_H_FILE)
endif("${HAVE_HTONLL}" STREQUAL "")

# we may compile in strict pedantic mode, but still want access to
# some POSIX functions.  test whether some symbols (below) are
# declared in addition to whether they resolve (above).
#
# we do this manually for now because CHECK_SYMBOL_EXISTS and
# CHECK_PROTOTYPE_EXISTS are both terribly broken.
#
# FIXME: wrap these into a BRLCAD_CHECK_SYMBOL() macro or similar.

# test for kill()
BRLCAD_FUNCTION_EXISTS(kill HAVE_KILL)
if("${HAVE_DECL_KILL}" MATCHES "^${HAVE_DECL_KILL}$")
  check_c_source_compiles("#include <sys/types.h>\n#include <signal.h>\nint main() { (void)kill; return 0; }" HAVE_DECL_KILL)
  if(HAVE_DECL_KILL)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_DECL_KILL 1\n")
  endif(HAVE_DECL_KILL)
endif("${HAVE_DECL_KILL}" MATCHES "^${HAVE_DECL_KILL}$")

# test for fileno()
BRLCAD_FUNCTION_EXISTS(fileno HAVE_FILENO)
if("${HAVE_DECL_FILENO}" MATCHES "^${HAVE_DECL_FILENO}$")
  check_c_source_compiles("#include <stdio.h>\nint main() { (void)fileno; return 0; }" HAVE_DECL_FILENO)
  if(HAVE_DECL_FILENO)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_DECL_FILENO 1\n")
  endif(HAVE_DECL_FILENO)
endif("${HAVE_DECL_FILENO}" MATCHES "^${HAVE_DECL_FILENO}$")

# test for snprintf()
BRLCAD_FUNCTION_EXISTS(snprintf HAVE_SNPRINTF)
if(NOT HAVE_SNPRINTF)
  BRLCAD_FUNCTION_EXISTS(_snprintf HAVE__SNPRINTF)
  if(HAVE__SNPRINTF)
    CONFIG_H_APPEND(BRLCAD "#define snprintf _snprintf\n")
  endif(HAVE__SNPRINTF)
endif(NOT HAVE_SNPRINTF)

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

# test for program_invocation_name global variable
if("${HAVE_PROGRAM_INVOCATION_NAME}" MATCHES "^${HAVE_PROGRAM_INVOCATION_NAME}$")
  check_c_source_compiles("#define _GNU_SOURCE\n#include <errno.h>\nint main() { return (program_invocation_name)?0:1; }" HAVE_PROGRAM_INVOCATION_NAME)
  if(HAVE_PROGRAM_INVOCATION_NAME)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_PROGRAM_INVOCATION_NAME 1\n")
  endif(HAVE_PROGRAM_INVOCATION_NAME)
endif("${HAVE_PROGRAM_INVOCATION_NAME}" MATCHES "^${HAVE_PROGRAM_INVOCATION_NAME}$")

# test for lrint existence
set(lrint_test "long int lrint(double); int main() {return lrint(3.14);}")
# this second test is mainly a test for BRLCAD_FUNCTION_EXISTS - can
# go away once behavior of the macro is solid
set(lrint_test_negative "long int lrint(double); int main() {return lrint(-1);}")
BRLCAD_FUNCTION_EXISTS(lrint HAVE_LRINT
  COMPILE_TEST_SRCS lrint_test lrint_test_negative
  REQUIRED_LIBS ${M_LIBRARY})
if(NOT HAVE_LRINT AND "${HAVE_WORKING_LRINT_MACRO}" MATCHES "^${HAVE_WORKING_LRINT_MACRO}$")
  set(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES ${M_LIBRARY})
  # NOTE: this must match the lrint #define in include/common.h
  check_c_source_compiles("#define lrint(_x) (((_x) < 0.0) ? ceil((_x)-0.5) : floor((_x)+0.5))\nint main() {return lrint(3.14);}" HAVE_WORKING_LRINT_MACRO)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
  if(HAVE_WORKING_LRINT_MACRO)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_WORKING_LRINT_MACRO 1\n")
  endif(HAVE_WORKING_LRINT_MACRO)
endif(NOT HAVE_LRINT AND "${HAVE_WORKING_LRINT_MACRO}" MATCHES "^${HAVE_WORKING_LRINT_MACRO}$")

# test for lrint() declaration
if(HAVE_LRINT AND "${HAVE_DECL_LRINT}" MATCHES "^${HAVE_DECL_LRINT}$")
  set(CMAKE_REQUIRED_LIBRARIES_BAK ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES ${M_LIBRARY})
  check_c_source_compiles("#include <math.h>\nint main() { (void)lrint(0); return 0; }" HAVE_DECL_LRINT)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_BAK})
  if(HAVE_DECL_LRINT)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_DECL_LRINT 1\n")
  endif(HAVE_DECL_LRINT)
endif(HAVE_LRINT AND "${HAVE_DECL_LRINT}" MATCHES "^${HAVE_DECL_LRINT}$")

# test for tgamma
set(tgamma_test "#include <math.h>\nint main() {double tga = tgamma(3.14); return 0;}")
BRLCAD_FUNCTION_EXISTS(tgamma HAVE_TGAMMA COMPILE_TEST_SRCS tgamma_test REQUIRED_LIBS ${M_LIBRARY})

#-----------------------------------------------------------------------
# check C99 and POSIX 200112L missing functions with a more
# rigorously-controlled compiler environment
if(ENABLE_POSIX_COMPLIANCE
    OR ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE)

  set(needed_FUNCS
    test_fdopen
    test_fileno
    test_realpath
    test_gethostname
    test_posix_memalign
    test_strcasecmp
    test_strncasecmp
    test_sigaction
    test_strtok_r
    )

  foreach(tprog ${needed_FUNCS})
    # remove "test_" from tprog
    STRING(REGEX REPLACE "test_" "" func ${tprog})
    STRING(TOUPPER "${func}" FUNC)

    TRY_COMPILE(success
      "${CMAKE_BINARY_DIR}/test_funcs"
      "${CMAKE_SOURCE_DIR}/misc/CMake/compat/${tprog}.c"
      CMAKE_FLAGS "${CMAKE_C_FLAGS}"
      )

    if(success)
      CONFIG_H_APPEND(BRLCAD "#define HAVE_WORKING_${FUNC}_FUNCTION 1\n")
      message("Try compile '${tprog}.c' (function ${func}) SUCCESS")
    else()
      message("Try compile '${tprog}.c' (function ${func}) FAIL")
    endif()

  endforeach()

endif()
#-----------------------------------------------------------------------

# On Windows, we need to check for hypot etc.  This test pertains
# to the windows specific config file, not CONFIG_H_FILE - hence,
# just run the test and it will be handled by configure_file later.
if(WIN32)

  # consider all warnings as errors (MSVC)
  # CHECK_C_FLAG(WX)
  # CHECK_CXX_FLAG(WX)

  CHECK_SYMBOL_EXISTS(hypot "math.h" HAVE_HYPOT)
  #If we don't have it, need to define it as _hypot in config_win
  if(NOT HAVE_HYPOT)
    set(hypot 1)
  endif(NOT HAVE_HYPOT)

  CHECK_SYMBOL_EXISTS(asinh "math.h" HAVE_ASINH)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_ASINH)
    set(asinh 1)
  endif(NOT HAVE_ASINH)

  CHECK_SYMBOL_EXISTS(isnan "math.h" HAVE_ISNAN)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_ISNAN)
    set(isnan 1)
  endif(NOT HAVE_ISNAN)

  CHECK_SYMBOL_EXISTS(isinf "math.h" HAVE_ISINF)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_ISINF)
    set(isinf 1)
  endif(NOT HAVE_ISINF)

  CHECK_SYMBOL_EXISTS(rint "math.h" HAVE_RINT)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_RINT)
    set(rint 1)
  endif(NOT HAVE_RINT)

  CHECK_SYMBOL_EXISTS(fmax "math.h" HAVE_FMAX)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_FMAX)
    set(fmax 1)
  endif(NOT HAVE_FMAX)

  CHECK_SYMBOL_EXISTS(nextafterf "math.h" HAVE_NEXTAFTERF)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_NEXTAFTERF)
    set(nextafterf 1)
  endif(NOT HAVE_NEXTAFTERF)

  CHECK_SYMBOL_EXISTS(nextafterl "math.h" HAVE_NEXTAFTERL)
  #If we don't have it, need to define it in config_win
  if(NOT HAVE_NEXTAFTERL)
    set(nextafterl 1)
  endif(NOT HAVE_NEXTAFTERL)

  BRLCAD_FUNCTION_EXISTS(_fseeki64 HAVE__FSEEKI64)
  BRLCAD_FUNCTION_EXISTS(_ftelli64 HAVE__FTELLI64)

endif(WIN32)

# Check whether we need to add import/export lines to libraries
if(MSVC)
  set(CPP_DLL_DEFINES 1)
endif(MSVC)

# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*          Stage 8 of 9 - Check for System Services       *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

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

# At the start, clear the src/other subdirs list so repeated
# configures will correctly add the required directories
set(SRC_OTHER_ADDED_DIRS "" CACHE STRING "initialize 3rd party sub-directories list" FORCE)
mark_as_advanced(SRC_OTHER_ADDED_DIRS)

macro(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endmacro(SetTargetFolder)

# Load some CMake macros to handle the special case of third party libraries.
include("${CMAKE_SOURCE_DIR}/misc/CMake/ThirdParty.cmake")

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
if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")

# *******************************************************************
if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")
  message("***********************************************************")
  message("*        Stage 9 of 9 - Define BRL-CAD Build Targets      *")
  message("***********************************************************")
endif(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt.prev")

# Make sure the flags that were cleared earlier are at least set
# to empty - CMake considers them "required internal variables".
set(CMAKE_REQUIRED_FLAG_VARS
  CMAKE_C_FLAGS
  CMAKE_CXX_FLAGS
  CMAKE_SHARED_LINKER_FLAGS
  CMAKE_EXE_LINKER_FLAGS
  )
foreach(req_var ${CMAKE_REQUIRED_FLAG_VARS})
  if(NOT DEFINED ${req_var})
    set(${req_var} "")
  endif(NOT DEFINED ${req_var})
endforeach(req_var ${CMAKE_REQUIRED_FLAG_VARS})

# Enable CTest Testing support - this is done after src/other is
# added to avoid incorporating src/other CTests into BRL-CAD's own
# testing.  TODO - probably should control this with an option.
include(CTest)
mark_as_advanced(BUILD_TESTING)

# We've done the toplevel configure steps, now add the subdirectories
macro(verbose_add_subdirectory root dir)
  if(NOT EXISTS "${BRLCAD_BINARY_DIR}/CMakeCache.txt.prev")
    if(NOT "${root}" STREQUAL "")
      message("-- Adding ${root}/${dir}...")
    else(NOT "${root}" STREQUAL "")
      message("-- Adding ${dir}...")
    endif(NOT "${root}" STREQUAL "")
  endif(NOT EXISTS "${BRLCAD_BINARY_DIR}/CMakeCache.txt.prev")
  add_subdirectory(${dir})
  if(NOT EXISTS "${BRLCAD_BINARY_DIR}/CMakeCache.txt.prev")
    if(NOT "${root}" STREQUAL "")
      message("-- Adding ${root}/${dir} - done")
    else(NOT "${root}" STREQUAL "")
      message("-- Adding ${dir} - done")
    endif(NOT "${root}" STREQUAL "")
  endif(NOT EXISTS "${BRLCAD_BINARY_DIR}/CMakeCache.txt.prev")
endmacro()

verbose_add_subdirectory("" src)
verbose_add_subdirectory("" include)
verbose_add_subdirectory("" sh)
verbose_add_subdirectory("" misc)
if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  verbose_add_subdirectory("" doc)
  verbose_add_subdirectory("" db)
  verbose_add_subdirectory("" bench)
  verbose_add_subdirectory("" regress)
else(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  CMAKEFILES(doc bench regress db)
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)

# Restore BRL-CAD configurations, if necessary
if(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Allowed BRL-CAD configuration types" FORCE)
endif(CMAKE_CONFIGURATION_TYPES AND NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "Debug;Release")

# *******************************************************************
# ***        Output and Summarize Results - Stage 9 of 9          ***
# *******************************************************************

#---------------------------------------------------------------------
# Record compile-time information we may want to report at run-time.
# Generate the include/conf files.

# COUNT - Count how many times the configuration has changed.  Currently
# we are detecting a configuration change by looking at the previous version
# of the CMakeCache.txt file and looking for changes between values stored
# there and settings in the current environment.  This has the disadvantage
# of not spotting completely new variables in the current environment, but
# the internal CMake variable list is too noisy so at the moment this looks
# like the best alternative we have.

if(EXISTS "${BRLCAD_BINARY_DIR}/include/conf/COUNT")
  include("${CMAKE_SOURCE_DIR}/misc/CMake/DiffCache.cmake")
  DIFF_CACHE_file()
  if(INCREMENT_COUNT_FILE)
    file(READ "${BRLCAD_BINARY_DIR}/include/conf/COUNT" buildCounter_raw)
    string(STRIP ${buildCounter_raw} buildCounter)
    math(EXPR buildCounter "${buildCounter} + 1")
    file(WRITE "${BRLCAD_BINARY_DIR}/include/conf/COUNT" "${buildCounter}\n")
  endif(INCREMENT_COUNT_FILE)
else(EXISTS "${BRLCAD_BINARY_DIR}/include/conf/COUNT")
  set(buildCounter 0)
  file(WRITE "${BRLCAD_BINARY_DIR}/include/conf/COUNT" "${buildCounter}\n")
endif(EXISTS "${BRLCAD_BINARY_DIR}/include/conf/COUNT")
DISTCLEAN("${BRLCAD_BINARY_DIR}/include/conf/COUNT")

# DATE - Increment this when we increment the COUNT, to avoid triggering
# excessive relinking (an RFC2822 timestamp changes every second).
file(READ ${CONFIG_TIMESTAMP_FILE} DATESTAMP)
string(STRIP ${DATESTAMP} DATESTAMP)
if(INCREMENT_COUNT_FILE OR NOT EXISTS "${BRLCAD_BINARY_DIR}/include/conf/DATE")
  file(WRITE "${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp" "\"${DATESTAMP}\"\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp" "${BRLCAD_BINARY_DIR}/include/conf/DATE")
  execute_process(COMMAND ${CMAKE_COMMAND} -E remove "${BRLCAD_BINARY_DIR}/include/conf/DATE.tmp")
endif(INCREMENT_COUNT_FILE OR NOT EXISTS "${BRLCAD_BINARY_DIR}/include/conf/DATE")
DISTCLEAN("${BRLCAD_BINARY_DIR}/include/conf/DATE")

# HOST
configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/report_hostname.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c")
if(NOT MSVC)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    "${CMAKE_BINARY_DIR}/CMakeTmp"
    "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c"
    OUTPUT_VARIABLE COMPILEMESSAGES)
else(NOT MSVC)
  set(CMAKE_REQUIRED_LIBRARIES ws2_32.lib)
  TRY_RUN(TIME_RESULT TIME_COMPILED
    "${CMAKE_BINARY_DIR}/CMakeTmp"
    "${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c"
    OUTPUT_VARIABLE COMPILEMESSAGES
    CMAKE_FLAGS "-DLINK_LIBRARIES:STRING=ws2_32.lib")
endif(NOT MSVC)
# Let CMake give us the proper newline character(s) for the current platform
file(APPEND "${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp" "\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp" "${BRLCAD_BINARY_DIR}/include/conf/HOST")
execute_process(COMMAND ${CMAKE_COMMAND} -E remove "${BRLCAD_BINARY_DIR}/include/conf/HOST.tmp")
DISTCLEAN("${BRLCAD_BINARY_DIR}/include/conf/HOST")

# USER
configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/report_username.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c")
TRY_RUN(TIME_RESULT TIME_COMPILED
  "${CMAKE_BINARY_DIR}/CMakeTmp"
  "${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c"
  OUTPUT_VARIABLE COMPILEMESSAGES)
# Let CMake give us the proper newline character(s) for the current platform
file(APPEND "${BRLCAD_BINARY_DIR}/include/conf/USER.tmp" "\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BRLCAD_BINARY_DIR}/include/conf/USER.tmp" "${BRLCAD_BINARY_DIR}/include/conf/USER")
execute_process(COMMAND ${CMAKE_COMMAND} -E remove "${BRLCAD_BINARY_DIR}/include/conf/USER.tmp")
DISTCLEAN("${BRLCAD_BINARY_DIR}/include/conf/USER")


# Write out version and compilation information

file(READ "${BRLCAD_BINARY_DIR}/include/conf/DATE"  BRLCAD_COMPILE_DATE)
file(READ "${BRLCAD_BINARY_DIR}/include/conf/HOST"  BRLCAD_COMPILE_HOST)
file(READ "${BRLCAD_BINARY_DIR}/include/conf/USER"  BRLCAD_COMPILE_USER)
file(READ "${BRLCAD_BINARY_DIR}/include/conf/COUNT"  BRLCAD_COMPILE_COUNT)

string(STRIP ${BRLCAD_COMPILE_DATE} BRLCAD_COMPILE_DATE)
string(STRIP ${BRLCAD_COMPILE_HOST} BRLCAD_COMPILE_HOST)
string(STRIP ${BRLCAD_COMPILE_USER} BRLCAD_COMPILE_USER)
string(STRIP ${BRLCAD_COMPILE_COUNT} BRLCAD_COMPILE_COUNT)

if(NOT BRLCAD_COMPILE_DATE)
  set(BRLCAD_COMPILE_DATE "\"date_unknown\"")
endif(NOT BRLCAD_COMPILE_DATE)
if(NOT BRLCAD_COMPILE_HOST)
  set(BRLCAD_COMPILE_HOST "\"host_unknown\"")
endif(NOT BRLCAD_COMPILE_HOST)
if(NOT BRLCAD_COMPILE_USER)
  set(BRLCAD_COMPILE_USER "\"user_unknown\"")
endif(NOT BRLCAD_COMPILE_USER)
if(NOT BRLCAD_COMPILE_COUNT)
  set(BRLCAD_COMPILE_COUNT -1)
endif(NOT BRLCAD_COMPILE_COUNT)

CONFIG_H_APPEND(BRLCAD "#define BRLCAD_COMPILE_DATE ${BRLCAD_COMPILE_DATE}\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_COMPILE_HOST ${BRLCAD_COMPILE_HOST}\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_COMPILE_USER ${BRLCAD_COMPILE_USER}\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_COMPILE_COUNT ${BRLCAD_COMPILE_COUNT}\n")

# If we're building on Windows with MSVC, use config_win.h file
if(MSVC)
  CONFIG_H_APPEND(BRLCAD "#include \"config_win.h\"\n")
endif(MSVC)

# Now that all the tests are done, configure the brlcad_config.h file:
CONFIG_H_APPEND(BRLCAD "#endif /* __CONFIG_H__ */\n")
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY ${CMAKE_PROJECT_NAME}_CONFIG_H_CONTENTS)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
configure_file(${CONFIG_H_FILE} "${BRLCAD_BINARY_DIR}/include/brlcad_config.h")

# Generate the windows specific file, if needed
if(WIN32)
  configure_file("${BRLCAD_SOURCE_DIR}/include/config_win.h.in" "${BRLCAD_BINARY_DIR}/include/config_win.h")
endif(WIN32)

# Create a 1 second sleep utility executable
try_compile(BUILD_SLEEP
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp"
  "${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/sleep.c"
  COMPILE_DEFINITIONS -DBRLCADBUILD -DHAVE_CONFIG_H
  -I"${CMAKE_CURRENT_BINARY_DIR}/include"
  -I"${CMAKE_CURRENT_SOURCE_DIR}/include"
  OUTPUT_VARIABLE BUILD_SLEEP_LOG
  COPY_FILE "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/sleep.bin")
if(NOT BUILD_SLEEP)
  message(FATAL_ERROR "BUILD_SLEEP failure: ${BUILD_SLEEP_LOG}")
endif(NOT BUILD_SLEEP)

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
  include("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_Summary.cmake")
endif(NOT BRLCAD_DISABLE_SUMMARY)


# *******************************************************************
# ***                      Timestamp Rules                        ***
# *******************************************************************
# TODO - As of CMake 2.8.11, the string command has a TIMESTAMP option
# that may be able to simplify some of the current timestamp system.
# We would need to be able to require 2.8.11 or newer, so there will
# be a bit of a wait, but it's something to look into.
if(NOT BRLCAD_IS_SUBBUILD)
  # Set up rules to print a timestamp string during build
  set(BUILD_DELTA_FILE "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_FILE")
  set(BUILD_DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_START")
  set(BUILD_DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_END")
  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/builddelta_start.c.in"
    "${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c")
  # The install instructions at the end of the message are tool specific - key
  # off of generators or build tools.
  if("${CMAKE_GENERATOR}" MATCHES "Make")
    set(INSTALL_LINE "Run 'make install' to begin installation into ${CMAKE_INSTALL_PREFIX}")
    set(BENCHMARK_LINE "Run 'make benchmark' to run the BRL-CAD Benchmark Suite")
  endif("${CMAKE_GENERATOR}" MATCHES "Make")
  if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(INSTALL_LINE "Run 'ninja install' to begin installation into ${CMAKE_INSTALL_PREFIX}")
    set(BENCHMARK_LINE "Run 'ninja benchmark' to run the BRL-CAD Benchmark Suite")
  endif("${CMAKE_GENERATOR}" MATCHES "Ninja")
  if("${CMAKE_GENERATOR}" MATCHES "Xcode")
    set(INSTALL_LINE "Run 'xcodebuild -target install' to begin installation into ${CMAKE_INSTALL_PREFIX}")
    set(BENCHMARK_LINE "Run 'xcodebuild -target benchmark' to run the BRL-CAD Benchmark Suite")
  endif("${CMAKE_GENERATOR}" MATCHES "Xcode")
  if(MSVC)
    # slightly misuse the lines for MSVC, since we don't usually do the
    # install/benchmark routine there. (Benchmarks aren't currently supported
    # in MSVC anyway.)
    set(INSTALL_LINE "To build, launch Visual Studio and open ${CMAKE_BINARY_DIR}/BRLCAD.sln")
    set(BENCHMARK_LINE "Build the ALL_BUILD target.  To create an NSIS installer, build the PACKAGE target")
  endif(MSVC)
  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/builddelta_end.c.in"
    "${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c")
  add_executable(printtimestamp misc/CMake/test_srcs/print_timestamp.c)
  set_target_properties(printtimestamp PROPERTIES FOLDER "Compilation Utilities")
  add_executable(buildtimestart "${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c")
  set_target_properties(buildtimestart PROPERTIES FOLDER "Compilation Utilities")
  add_executable(buildtimeend "${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c")
  set_target_properties(buildtimeend PROPERTIES FOLDER "Compilation Utilities")
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
  set_target_properties(timestamp PROPERTIES FOLDER "Compilation Utilities")
  add_custom_target(buildtimedelta ALL
    COMMAND buildtimeend
    COMMAND ${CMAKE_COMMAND} -E remove ${BUILD_DELTA_START}
    DEPENDS buildtimeend
    )
  set_target_properties(buildtimedelta PROPERTIES FOLDER "Compilation Utilities")
endif(NOT BRLCAD_IS_SUBBUILD)

# We want the timestamp to come first, so make ALL library,
# exec and external targets depend on timestamp (except for
# the exec target printtimestamp, which is needed by timestamp)
# Similarly, buildtimedelta needs to depend on every target.
if(NOT BRLCAD_IS_SUBBUILD)
  get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
  get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
  get_property(CMAKE_CUSTOM_TARGET_LIST GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST)
  get_property(CMAKE_EXTERNAL_TARGET_LIST GLOBAL PROPERTY CMAKE_EXTERNAL_TARGET_LIST)
  mark_as_advanced(CMAKE_LIBRARY_TARGET_LIST)
  mark_as_advanced(CMAKE_EXEC_TARGET_LIST)
  mark_as_advanced(CMAKE_CUSTOM_TARGET_LIST)
  mark_as_advanced(CMAKE_EXTERNAL_TARGET_LIST)
  list(REMOVE_DUPLICATES CMAKE_LIBRARY_TARGET_LIST)
  list(REMOVE_DUPLICATES CMAKE_EXEC_TARGET_LIST)
  list(REMOVE_DUPLICATES CMAKE_CUSTOM_TARGET_LIST)
  if(CMAKE_EXTERNAL_TARGET_LIST)
    list(REMOVE_DUPLICATES CMAKE_EXTERNAL_TARGET_LIST)
  endif(CMAKE_EXTERNAL_TARGET_LIST)
  foreach(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
    if(TARGET ${libtarget})
      add_dependencies(${libtarget} timestamp)
      add_dependencies(buildtimedelta ${libtarget})
    endif(TARGET ${libtarget})
  endforeach(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
  foreach(exectarget ${CMAKE_EXEC_TARGET_LIST})
    if(NOT CMAKE_BUILD_TYPE)
      get_target_property(target_location ${exectarget} LOCATION)
    else(NOT CMAKE_BUILD_TYPE)
      get_target_property(target_location ${exectarget} LOCATION_${CMAKE_BUILD_TYPE})
    endif(NOT CMAKE_BUILD_TYPE)
    if(target_location)
      if(NOT ${exectarget} MATCHES "printtimestamp" AND NOT ${exectarget} MATCHES "buildtimestart")
	add_dependencies(${exectarget} timestamp)
      endif()
      add_dependencies(buildtimedelta ${exectarget})
    endif(target_location)
  endforeach(exectarget ${CMAKE_EXEC_TARGET_LIST})
  foreach(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
    if(NOT ${custtarget} MATCHES "timestamp")
      add_dependencies(${custtarget} timestamp)
    endif()
    if(NOT ${custtarget} MATCHES "buildtimedelta")
      get_target_property(not_in_all ${custtarget} EXCLUDE_FROM_ALL)
      get_target_property(not_in_default ${custtarget} EXCLUDE_FROM_DEFAULT_BUILD)
      if(NOT not_in_all AND NOT not_in_default)
	add_dependencies(buildtimedelta ${custtarget})
      endif(NOT not_in_all AND NOT not_in_default)
    endif(NOT ${custtarget} MATCHES "buildtimedelta")
  endforeach(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
  foreach(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
    get_target_property(target_confcmd ${externaltarget} _EP_CONFIGURE_COMMAND)
    if(target_confcmd)
      add_dependencies(${externaltarget} timestamp)
      add_dependencies(buildtimedelta ${externaltarget})
    endif(target_confcmd)
  endforeach(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})

endif(NOT BRLCAD_IS_SUBBUILD)


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
  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/multiconfig_path_clean.cmake.in" "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_clean.cmake" @ONLY)
  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/multiconfig_path_read.cmake.in" "${CMAKE_BINARY_DIR}/CMakeTmp/multiconfig_path_read.cmake" @ONLY)
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

  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/source_archive_setup.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake" @ONLY)

  option(BRLCAD_VERSIONED_RPM_NAME "Incorporate BRL-CAD version into RPM package name" OFF)
  mark_as_advanced(BRLCAD_VERSIONED_RPM_NAME)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BRL-CAD - a powerful cross-platform open source solid modeling system")
  set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
  set(CPACK_PACKAGE_VERSION_MAJOR ${BRLCAD_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${BRLCAD_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${BRLCAD_VERSION_PATCH})
  if(BRLCAD_VERSIONED_RPM_NAME)
    set(CPACK_RPM_PACKAGE_NAME "BRL-CAD_${BRLCAD_VERSION}")
  endif(BRLCAD_VERSIONED_RPM_NAME)

  if(UNIX)
    set(CPACK_STRIP_FILES TRUE)
  endif(UNIX)

  if(UNIX)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
  endif(UNIX)

  if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR TGZ TBZ2)

    find_program(RPMBUILD_EXEC rpmbuild)
    mark_as_advanced(RPMBUILD_EXEC)
    if(RPMBUILD_EXEC)
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
      #set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}")
      #if(EXISTS /etc/redhat-release)
      #	file(READ /etc/redhat-release REDHAT_RELEASE)
      #	string(REGEX MATCH "[0-9]+" REDHAT_VERSION ${REDHAT_RELEASE})
      #	string(REGEX MATCH "Enterprise Linux" LINUX_DIST_TYPE ${REDHAT_RELEASE})
      #	if(LINUX_DIST_TYPE)
      #		set(LINUX_DIST_TYPE "el")
      #	else(LINUX_DIST_TYPE)
      #		set(LINUX_DIST_TYPE "rh")
      #	endif(LINUX_DIST_TYPE)
      #	set(CPACK_RPM_PACKAGE_RELEASE 1.${LINUX_DIST_TYPE}${REDHAT_VERSION})
      #else(EXISTS /etc/redhat-release)
      #	#set(CPACK_RPM_PACKAGE_RELEASE 2)
      #endif(EXISTS /etc/redhat-release)
    endif(RPMBUILD_EXEC)
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
    if(NOT CPACK_RPM_PACKAGE_RELEASE)
      set(CPACK_RPM_PACKAGE_RELEASE 1)
    endif(NOT CPACK_RPM_PACKAGE_RELEASE)
    if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_x86_${CMAKE_SYSTEM_NAME}_${CPACK_RPM_PACKAGE_RELEASE}")
    else(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_PROCESSOR}_${CMAKE_SYSTEM_NAME}_${CPACK_RPM_PACKAGE_RELEASE}")
    endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  else(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
  endif(UNIX AND NOT APPLE)

  if(WIN32)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD")
    set(CPACK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    set(CPACK_DATA_DIR "${DATA_DIR}")
    # There is a bug in NSI that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backslashes.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\brlcad.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\uninstall.ico")
    set(CPACK_NSIS_DISPLAY_NAME "BRL-CAD")
    set(CPACK_NSIS_MODIFY_PATH ON)
    if(BRLCAD_ENABLE_WIX)
      set(CPACK_GENERATOR ${CPACK_GENERATOR} WIX)
      set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/misc/wix/License.rtf")
      set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_product.ico")
      set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_banner.bmp")
      set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_dialog.bmp")
    endif(BRLCAD_ENABLE_WIX)
  endif(WIN32)
  if(CMAKE_CL_64)
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_x64")
  endif(CMAKE_CL_64)

  set(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "brlcad-${BRLCAD_VERSION}")
  set(CPACK_SOURCE_IGNORE_FILES "\\\\.svn/")

  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/BRLCAD_CPackOptions.cmake.in" "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake" @ONLY)
  set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")

  include(CPack)

  # Some files to ignore for distcheck.  For this case
  # only, we add CMakeLists.txt (others are handled
  # by add_subdirectory wrapper
  set(toplevel_ignore_files
    BUGS
    ChangeLog
    TODO
    configure
    CMakeLists.txt
    CTestConfig.cmake
    )
  CMAKEFILES(${toplevel_ignore_files})

  # Handle some toplevel distclean listings
  DISTCLEAN("${CMAKE_BINARY_DIR}/cmakefiles.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/cmakedirs.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/cmake_install.cmake")
  DISTCLEAN("${CMAKE_BINARY_DIR}/install_manifest.txt")
  DISTCLEAN("${CMAKE_BINARY_DIR}/OPTIONS")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeFiles")
  DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp")
  DISTCLEAN("${CMAKE_BINARY_DIR}/configure.new")
  DISTCLEAN("${CMAKE_BINARY_DIR}/INSTALL.new")
  DISTCLEAN("${CMAKE_BINARY_DIR}/include/brlcad_config.h.in")
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
  include("${CMAKE_SOURCE_DIR}/misc/CMake/Distcheck.cmake")

  # Define some custom distcheck targets for distcheck-full (not run by default)
  #
  # CREATE_DISTCHECK(TARGET_SUFFIX CMAKE_OPTS source_dir build_dir install_dir [custom_template])
  CREATE_DISTCHECK(default_build_type "-DBRLCAD_BUNDLED_LIBS=AUTO" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
  CREATE_DISTCHECK(no_tk "-DCMAKE_BUILD_TYPE=Debug -DBRLCAD_BUNDLED_LIBS=BUNDLED -DBRLCAD_ENABLE_TK=OFF" "${CPACK_SOURCE_PACKAGE_FILE_NAME}" "build" "install")
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

# ----------------------------------------------------------------------------
# If options have been defined that we are going to document, by now they've
# been documented.  Make an updated copy of the "INSTALL" file and see whether
# anything has changed.
#
# Although the general principle of this build is to leave the src dir
# pristine after configuration and compilation, the INSTALL and configure files
# represent one of two cases where the default behavior is to alter the files
# if changes have occurred.  (The other is cached generated outputs from tools
# such as re2c and lemon.) For the purposes of this project, a given
# option's documentation is defined in the CMakeLists.txt source file with
# the option definition logic - the text in INSTALL is simply an output from
# that, and is never the correct place for manual changes.
#
# As with generated outputs, this auto-updating behavior can be overridden
# by setting the CMake variable LOCKED_SOURCE_DIR to ON - in that case, a
# warning about out-of-sync files is printed but no automatic updating takes
# place in the source tree.

set(CONFIG_FATAL_ERROR 0)

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
   information.\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  "${BRLCAD_SOURCE_DIR}/INSTALL" "${BRLCAD_BINARY_DIR}/INSTALL.new"
  RESULT_VARIABLE IS_DIFFERENT OUTPUT_VARIABLE DIFF_OUTPUT ERROR_VARIABLE
  DIFF_ERROR)
if(IS_DIFFERENT)
  set(CONFIG_FATAL_ERROR 1)
  message("\n\"${BRLCAD_SOURCE_DIR}/INSTALL\" is out of date.  An updated version has been generated at \"${BRLCAD_BINARY_DIR}/INSTALL.new\"\nTo clear this warning, replace \"${BRLCAD_SOURCE_DIR}/INSTALL\" with \"${BRLCAD_BINARY_DIR}/INSTALL.new\"\n")
endif(IS_DIFFERENT)

# Do the same thing for the configure shell script - finish it and check
# for differences.
file(READ "${CMAKE_SOURCE_DIR}/misc/CMake/configure_suffix.sh" CONFIG_SUFFIX)
file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "${CONFIG_SUFFIX}")
# Compare
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
  "${BRLCAD_SOURCE_DIR}/configure" "${BRLCAD_BINARY_DIR}/configure.new"
  RESULT_VARIABLE IS_CONFIG_SH_DIFFERENT OUTPUT_VARIABLE DIFF_OUTPUT ERROR_VARIABLE
  DIFF_ERROR)
if(IS_CONFIG_SH_DIFFERENT)
  set(CONFIG_FATAL_ERROR 1)
  message("\n\"${BRLCAD_SOURCE_DIR}/configure\" is out of date. An updated version has been generated at \"${BRLCAD_BINARY_DIR}/configure.new\"\nTo clear this warning, replace \"${BRLCAD_SOURCE_DIR}/configure\" with \"${BRLCAD_BINARY_DIR}/configure.new\"\n")
endif(IS_CONFIG_SH_DIFFERENT)

if(CONFIG_FATAL_ERROR)
  message(FATAL_ERROR "Configure haulted.")
endif(CONFIG_FATAL_ERROR)


# Because the build-time-delta needs a configure-file but comes at the
# end of the CMake configure, the preparation of the final distclean
# list must account for it.  Also, the distclean script itself must
# be added to the list, as it is generated by CMake
DISTCLEAN("${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c")
get_property(CMAKE_DISTCLEAN_TARGET_LIST GLOBAL PROPERTY CMAKE_DISTCLEAN_TARGET_LIST)
list(REMOVE_DUPLICATES CMAKE_DISTCLEAN_TARGET_LIST)
configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/distclean.cmake.in" "${BRLCAD_BINARY_DIR}/distclean.cmake" @ONLY)
if("${CMAKE_GENERATOR}" MATCHES "Make")
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E echo "Running make clean..."
    COMMAND ${CMAKE_COMMAND} -E chdir "${BRLCAD_BINARY_DIR}" $(MAKE) clean
    COMMAND ${CMAKE_COMMAND} -E echo "Running make clean... done."
    COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake"
    )
elseif("${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E echo "Running make clean..."
    COMMAND ${CMAKE_COMMAND} -E chdir "${BRLCAD_BINARY_DIR}" ninja -t clean
    COMMAND ${CMAKE_COMMAND} -E echo "Running make clean... done."
    COMMAND ${CMAKE_COMMAND} -P "${BRLCAD_BINARY_DIR}/distclean.cmake"
    )
else("${CMAKE_GENERATOR}" MATCHES "Make")
  add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E echo "Not implemented for generator ${CMAKE_GENERATOR}"
    )
endif("${CMAKE_GENERATOR}" MATCHES "Make")
if(TARGET distclean)
  set_target_properties(distclean PROPERTIES FOLDER "Compilation Utilities")
endif(TARGET distclean)

# Not sure what this is - created automatically by CMake in MSVC?
SetTargetFolder(core "CMakePredefinedTargets")

# Have touched every file and directory that we are going to touch with the
# build system - time to write out our lists for future processing.
get_property(CMAKE_IGNORE_FILES GLOBAL PROPERTY CMAKE_IGNORE_FILES)
get_property(CMAKE_IGNORE_DIRS GLOBAL PROPERTY CMAKE_IGNORE_DIRS)
string(REPLACE ";" "\n" CMAKE_IGNORE_FILES "${CMAKE_IGNORE_FILES}")
string(REPLACE ";" "\n" CMAKE_IGNORE_DIRS "${CMAKE_IGNORE_DIRS}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake" "${CMAKE_IGNORE_FILES}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmakedirs.cmake" "${CMAKE_IGNORE_DIRS}")

#Done with all really time-consuming steps - do the configure time delta
if(NOT BRLCAD_IS_SUBBUILD)
  set(DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_END")
  configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/timedelta_end.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c")
  TRY_RUN(TIME_RESULT TIME_COMPILED
    "${CMAKE_BINARY_DIR}/CMakeTmp"
    "${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c"
    OUTPUT_VARIABLE COMPILEMESSAGES)
  file(READ ${DELTA_END} CONFIG_TIME_MSG)
  string(STRIP "${CONFIG_TIME_MSG}" CONFIG_TIME_MSG)
  set(CONFIG_TIME_MSG_LABEL "Elapsed configuration time")
  string(LENGTH "${CONFIG_TIME_MSG_LABEL}" CURRENTLENGTH)
  while(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
    set(CONFIG_TIME_MSG_LABEL "${CONFIG_TIME_MSG_LABEL}.")
    string(LENGTH "${CONFIG_TIME_MSG_LABEL}" CURRENTLENGTH)
  endwhile(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
  message("${CONFIG_TIME_MSG_LABEL}..: ${CONFIG_TIME_MSG}")
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
_add_test(NAME "NOTE:\\ some\\ 'test'\\ tests\\ are\\ expected\\ to\\ fail,\\ 'regress'\\ must\\ pass" COMMAND ${CMAKE_COMMAND} --build . --target print-warning-message)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
