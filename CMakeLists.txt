#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010 United States Government as represented by
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
# This file define the toplevel CMake build logic for BRL-CAD.  
# As best is reasonably possible, proper ordering and
# separation of tests and settings should be maintained per the
# recommended standard layout.  The tests should be added to the
# rather clearly labeled sections below so that they are as
# follows:
#
#     0) information on the package and toplevel CMake settings
#     1) define top level options
#     2) check programs
#     3) check libraries
#     4) check headers
#     5) check types/structures
#     6) check compiler characteristics
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
# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(BRLCAD)

# Repeated runs of cmake wil result in this variable being set during
# the Find* routines, which will return libraries from previous
# installs. Even if never set using CACHE by the CMakeLists.txt file,
# /usr/local gets written to the CACHE file.  If BRL-CAD happened to
# be installed there, this would very probably cause the same trouble
# being seen with detected CMAKE_INSTALL_PREFIX libs in other directories.
# Hard erase this variable's setting in the cache.
SET(CMAKE_INSTALL_PREFIX "" CACHE INTERNAL "Keep CMAKE_INSTALL_PREFIX unset in CACHE so repeated Find* operations return consistent results")

#---------------------------------------------------------------------
# By default (as of version 2.8.2) CMake does not provide access to 
# global lists of executable and library targets.  This is useful
# in a number of situations related to formulating custom rules and
# target dependency management.  To avoid the necessity of replacing
# add_library and add_executable calls with custom macros, override
# the function names and call the _add_* functions to access the CMake
# functionality previously available under the add_* functions. See
# http://www.cmake.org/pipermail/cmake/2010-September/039388.html
function(add_library name)
   _add_library(${name} ${ARGN})
	IF(${name} MATCHES "^lib*")
		set_target_properties(${name} PROPERTIES PREFIX "")
	ENDIF(${name} MATCHES "^lib*")
   SET(CMAKE_LIBRARY_TARGET_LIST "${CMAKE_LIBRARY_TARGET_LIST};${name}" CACHE STRING "list of lib targets" FORCE)
   get_target_property(targetfile ${name} LOCATION)
   STRING(REGEX REPLACE "liblib" "lib" targetvar "${targetfile}")
   SET(CMAKE_LIBRARY_TARGET_FILE_LIST "${CMAKE_LIBRARY_TARGET_FILE_LIST};${targetfile}" CACHE STRING "list of lib target files" FORCE)
endfunction(add_library)

function(add_executable name)
   SET(CMAKE_EXEC_TARGET_LIST "${CMAKE_EXEC_TARGET_LIST};${name}" CACHE STRING "list of exec targets" FORCE)
   _add_executable(${name} ${ARGN})
   get_target_property(targetfile ${name} LOCATION)
   SET(CMAKE_EXEC_TARGET_FILE_LIST "${CMAKE_EXEC_TARGET_FILE_LIST};${targetfile}" CACHE STRING "list of exec target files" FORCE)
endfunction(add_executable)

function(add_custom_target name)
   SET(CMAKE_CUSTOM_TARGET_LIST "${CMAKE_CUSTOM_TARGET_LIST};${name}" CACHE STRING "list of custom targets" FORCE)
   _add_custom_target(${name} ${ARGN})
   get_target_property(targetfile ${name} LOCATION)
	SET(CMAKE_CUSTOM_TARGET_FILE_LIST "${CMAKE_CUSTOM_TARGET_FILE_LIST};${targetfile}" CACHE STRING "list of custom target files" FORCE)
endfunction(add_custom_target)



#---------------------------------------------------------------------
# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
SET(CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR};${CMAKE_MODULE_PATH}")

# Load utility macros that will be used throughout the BRL-CAD
# build logic
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_Util.cmake)

#---------------------------------------------------------------------
# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables 
# and set them to "C" so things like date output are as expected. 
SET(_orig_lc_all      $ENV{LC_ALL})
SET(_orig_lc_messages $ENV{LC_MESSAGES})
SET(_orig_lang        $ENV{LANG})
IF(_orig_lc_all)
	SET(ENV{LC_ALL}      C)
ENDIF(_orig_lc_all)
IF(_orig_lc_messages)
	SET(ENV{LC_MESSAGES} C)
ENDIF(_orig_lc_messages)
IF(_orig_lang)
	SET(ENV{LANG}        C)
ENDIF(_orig_lang)

#---------------------------------------------------------------------
# See HACKING for details on how to properly update the version
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MAJOR BRLCAD_MAJOR_VERSION)
STRING(STRIP ${BRLCAD_MAJOR_VERSION} BRLCAD_MAJOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MINOR BRLCAD_MINOR_VERSION)
STRING(STRIP ${BRLCAD_MINOR_VERSION} BRLCAD_MINOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/PATCH BRLCAD_PATCH_VERSION)
STRING(STRIP ${BRLCAD_PATCH_VERSION} BRLCAD_PATCH_VERSION)

SET(CAD_VERSION "${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
SET(BRLCAD_VERSION "${CAD_VERSION}")

#---------------------------------------------------------------------
# Write out entries to populate a tm struct to be used for time deltas 
# later
SET(DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_START")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/timedelta_start.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c
	OUTPUT_VARIABLE COMPILEMESSAGES)

#---------------------------------------------------------------------
# Unfortunately, CMake doesn't give you variables with current day, 
# month, etc.  There are several possible approaches to this, but most 
# (e.g. the date command) are not cross platform. We build a small C 
# file which writes out the needed values to files in the build 
# directory. Those files are then read and stripped by CMake.
SET(CONFIG_TIME_DAY_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_DAY")
SET(CONFIG_TIME_MONTH_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_MONTH")
SET(CONFIG_TIME_YEAR_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_YEAR")
SET(CONFIG_TIMESTAMP_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIMESTAMP")
FILE(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include")
FILE(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include/conf")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/time.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/time.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/time.c
	OUTPUT_VARIABLE COMPILEMESSAGES)
IF(TIME_RESULT MATCHES "^0$")
	FILE(READ ${CONFIG_TIME_DAY_FILE} CONFIG_DAY)
	STRING(STRIP ${CONFIG_DAY} CONFIG_DAY)
	FILE(READ ${CONFIG_TIME_MONTH_FILE} CONFIG_MONTH)
	STRING(STRIP ${CONFIG_MONTH} CONFIG_MONTH)
	FILE(READ ${CONFIG_TIME_YEAR_FILE} CONFIG_YEAR)
	STRING(STRIP ${CONFIG_YEAR} CONFIG_YEAR)
	SET(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
	FILE(READ ${CONFIG_TIMESTAMP_FILE} CONFIG_TS)
	STRING(STRIP ${CONFIG_TS} CONFIG_TS)
ELSE(TIME_RESULT MATCHES "^0$")
	MESSAGE(ERROR "Code to determine current date and time failed!\n")
ENDIF(TIME_RESULT MATCHES "^0$")

#---------------------------------------------------------------------
# print out the title with a pretty box computed to wrap around
BOX_PRINT("*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***" "*")

#---------------------------------------------------------------------
# For NFS volumes, to ensure proper file creation.
IF(NOT WIN32)
	EXEC_PROGRAM(umask ARGS 022 OUTPUT_VARIABLE exec_out)
ENDIF(NOT WIN32)

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it 
SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};/usr/local)
SET(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES};/usr/local/include)

#---------------------------------------------------------------------
# Set build type here. Doing this wrong, see http://www.cmake.org/Wiki/CMake_Useful_Variables
# for a more complete list of types and the mechanism to define new ones - dunno if 
# set_property can be made to work here or not.  Reason to do this is in CMake gui it gives
# the list of available build types as a drop-down menu
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Basic" "Debug" "Release")
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Basic" CACHE STRING "Set build type" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

#---------------------------------------------------------------------
# The location in which to install BRLCAD. Need a good Debug location 
# for Windows 
IF(NOT BRLCAD_PREFIX)
	IF(NOT WIN32)
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(BRLCAD_PREFIX "/usr/brlcad/rel-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			SET(BRLCAD_PREFIX "${BRLCAD_SOURCE_DIR}/../brlcad-install")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		if (NOT BRLCAD_PREFIX)
			SET(BRLCAD_PREFIX "/usr/brlcad")
		endif (NOT BRLCAD_PREFIX)
	ELSE(NOT WIN32)
		SET(BRLCAD_PREFIX "C:/BRL-CAD")
	ENDIF(NOT WIN32)
ENDIF(NOT BRLCAD_PREFIX)
SET(CMAKE_INSTALL_PREFIX "${BRLCAD_PREFIX}")


#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in 
# the build directory AND install directory.  Thanks to plplot for 
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
	SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif(NOT APPLE)
SET(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to 
# directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable 
# files will be placed after building but prior to install.
IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
	SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

#---------------------------------------------------------------------
# Configure install locations.  It may be that this should be made
# into its own .cmake file and included in each library, to allow
# for more flexibility in using individual libraries in other projects.
MACRO(INSTALL_DIR upper suffix)
	IF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR)
		SET(${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR ${CMAKE_INSTALL_PREFIX}/${suffix})
		SET(${upper}_DIR ${suffix})
	ENDIF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR)
ENDMACRO(INSTALL_DIR upper suffix)

# The location in which to install BRLCAD executables.
INSTALL_DIR(BIN bin)

# The location in which to install BRLCAD header files.
INSTALL_DIR(INCLUDE include)

# The location in which to install BRL-CAD libraries.
INSTALL_DIR(LIB lib)

# The location in which to install BRL-CAD configuration files.
INSTALL_DIR(CONF etc)

# The location in which to install CMake scripts for packaging BRLCAD.
INSTALL_DIR(PACKAGE lib)

# The location in which to install BRL-CAD data files 
IF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR)
	IF (${CMAKE_INSTALL_PREFIX} MATCHES "^/usr[/]*$")
		INSTALL_DIR(DATA "share/brlcad")
	ENDIF()
	IF (${CMAKE_INSTALL_PREFIX} MATCHES "^/usr/local[/]*$")
		INSTALL_DIR(DATA "share/brlcad")
	ENDIF()
	INSTALL_DIR(DATA "share")
ENDIF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR)

# The location in which to install BRL-CAD Manual pages
INSTALL_DIR(MAN ${DATA_DIR}/man)

# The location in which to install BRL-CAD documentation files 
INSTALL_DIR(DOC ${DATA_DIR}/doc)

# The location in which to install BRL-CAD doxygen documentation helper
# files.
INSTALL_DIR(DOXYGEN ${DOC_DIR}/doxygen)

#---------------------------------------------------------------------
# We will need a brlcad_config.h.in file to hold all the #cmakedefine 
# statements, which will in turn be used to generate a brlcad_conf.h 
# file.  In autotools this process is handled by autoheader - in the 
# case of CMake we wrap the CHECK_* functions and the creation of the 
# entry in the brlcad_config.h.in file into one step via a macro.  To 
# run this macro, the config.h.in file must first be present, which
# we take care of here:

SET(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** Define statements for CMake ****/\n")
FILE(APPEND ${CONFIG_H_FILE} "#ifndef __CONFIG_H__\n")
FILE(APPEND ${CONFIG_H_FILE} "#define __CONFIG_H__\n")

# If we're building on Windows, start off with the config_win.h file
IF(WIN32)
	FILE(APPEND ${CONFIG_H_FILE} "#include \"config_win.h\"\n")
ENDIF(WIN32)

# Set up some of the define statements for path information and other basics
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE \"brlcad\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"http://brlcad.org\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"BRL-CAD\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"BRL-CAD ${BRLCAD_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"brlcad\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define BRLCAD_DATA \"${BRLCAD_INSTALL_DATA_DIR}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define BRLCAD_ROOT \"${BRLCAD_PREFIX}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_VERSION \"${BRLCAD_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define VERSION \"${BRLCAD_VERSION}\"\n")

IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
	FILE(APPEND ${CONFIG_H_FILE} "#define DEBUG 1\n")
ENDIF()

# *******************************************************************
# ***        Top Level Configure Options - Stage 1 of 9           ***
# *******************************************************************
#
# Now we define the various options for BRL-CAD - ways to enable and
# disable features, select which parts of the system to build, etc.  
# As much as possible, sane default options are either selected or
# detected.

# Build shared libs by default.  Mark this as advanced - turning off
# ALL shared library building is unlikely to result in a working build
# and is not a typical configuration.  Note that turning this option off
# will not disable libraries specifically added as SHARED.
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)
MARK_AS_ADVANCED(BUILD_SHARED_LIBS)

# Build static libs by default.  Standard practice is to build and
# install both dynamic and static versions of libraries.  This option
# is off by default in Debug configuration.  Note that turning this
# option off will not disable libraries specifically added as STATIC
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# Turn on just the benchmark build.  This is not an exclusive flag -
# enabling this does not force off other explicitly enabled targets.
OPTION(BRLCAD-ENABLE_BENCHMARK_ONLY "Enable just the benchmark target and its requirements." OFF)
#MARK_AS_ADVANCED(BRLCAD-ENABLE_BENCHMARK_ONLY)

# Enable the librtserver build
OPTION(BRLCAD-ENABLE_RTSERVER "Enable the rtserver target." ON)

# Turn on just the librtserver build.  This is not an exclusive flag -
# enabling this does not force off other explicitly enabled targets.
OPTION(BRLCAD-ENABLE_RTSERVER_ONLY "Enable just the rtserver target and its requirements." OFF)
#MARK_AS_ADVANCED(BRLCAD-ENABLE_RTSERVER_ONLY)
IF(BRLCAD-ENABLE_RTSERVER_ONLY)
   SET(BRLCAD-ENABLE_RTSERVER ON)
ENDIF(BRLCAD-ENABLE_RTSERVER_ONLY)

# Global third party controls - these options enable and disable ALL
# local copies of libraries in src/other.  For finer grain control,
# leave both of these options off and options defined by the
# src/other CMakeLists.txt file will come into play.  The default
# action is to test system libs and enable local copies of any
# either not found or not meeting requirements.  Forcing all
# libraries off is not usually recommended unless attempting to
# build packages for a distribution.  If both of these options are
# on the enabling of local packages is the "senior" option and will
# force the system libs option to off.
OPTION(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY "Use only the system's libraries." OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY)
OPTION(BRLCAD-ENABLE_ALL_LOCAL_LIBS "Build all the local copies of libraries." OFF)
IF(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY AND BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	SET(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY OFF CACHE BOOL "Use only the system's libraries." FORCE)
ENDIF(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY AND BRLCAD-ENABLE_ALL_LOCAL_LIBS)

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building.
# OPTION(BRLCAD-BUILD_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)

# Install example BRL-CAD Geometry Files
OPTION(BRLCAD-INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)

# Enable features requiring X11
if(WIN32)
	OPTION(BRLCAD-ENABLE_X11 "Use X11." OFF)
else()
	OPTION(BRLCAD-ENABLE_X11 "Use X11." ON)
endif(WIN32)
if (BRLCAD-ENABLE_X11)
	SET(BRLCAD-BUILD_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
	SET(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif (BRLCAD-ENABLE_X11)

# Enable/disable features requiring the Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tk
OPTION(BRLCAD-ENABLE_TK "Enable features requiring the Tk toolkit" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_TK)
IF(NOT WIN32)
	IF (APPLE)
		IF(NOT BRLCAD-ENABLE_X11 AND NOT BRLCAD-ENABLE_AQUA)
			SET(BRLCAD-ENABLE_TK OFF)
		ENDIF(NOT BRLCAD-ENABLE_X11 AND NOT BRLCAD-ENABLE_AQUA)
	ELSE (APPLE)
		IF(NOT BRLCAD-ENABLE_X11)
			SET(BRLCAD-ENABLE_TK OFF)
		ENDIF(NOT BRLCAD-ENABLE_X11)
	ENDIF(APPLE)
ENDIF(NOT WIN32)

# Enable features requring OPENGL
OPTION(BRLCAD-ENABLE_OPENGL "Use OpenGL." ON)

# Enable RTGL.  Requires an enabled OpenGL.
# IF(BRLCAD-ENABLE_OPENGL)
#    OPTION(BRLCAD-ENABLE_RTGL "Enable experimental RTGL code." ON)
# ENDIF(BRLCAD-ENABLE_OPENGL)

# There are extra documentation files available requiring Docbook
# They are quite useful in graphical interfaces, but also add considerably
# to the overall build time.  By default, PDF generation is off, and html/man
# generation (which produces files used in the GUIs) is on.
OPTION(BRLCAD-BUILD_EXTRADOCS "Build Docbook documentation" ON)
OPTION(BRLCAD-BUILD_EXTRADOCS_PDF "Build PDF output from Docbook documentation" OFF)
OPTION(BRLCAD-BUILD_EXTRADOCS_PDF_MAN "Build PDF output from Docbook documentation for man pages" ${BRLCAD-BUILD_EXTRADOCS_PDF})

#----------------------------------------------------------------------
# The following are fine-grained options for enabling/disabling compiler
# and source code definition settings.  Typically these are set to 
# various configurations by the toplevel CMAKE_BUILD_TYPE setting, but 
# can also be individually set.

# Enable/disable runtime debugging - these are protections for
# minimizing the possibility of corrupted data files.  Generally
# speaking these should be left on.
OPTION(BRLCAD-ENABLE_RUNTIME_DEBUG "Enable run-time debug checking." ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_RUNTIME_DEBUG)
IF(NOT BRLCAD-ENABLE_RUNTIME_DEBUG)
	MESSAGE("}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
	MESSAGE("While disabling run-time debugging should increase")
	MESSAGE("performance, it will likewise remove several")
	MESSAGE("data-protection safeguards that are in place to")
	MESSAGE("minimize the possibility of corrupted data files")
	MESSAGE("in the inevitable event of a user encountering a bug.")
	MESSAGE("You have been warned.  Proceed at your own risk.")
	MESSAGE("{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{")
	FILE(APPEND  ${CONFIG_H_FILE} "/*Define to not do anything for macros that only bomb on a fatal error. */\n")
	FILE(APPEND  ${CONFIG_H_FILE} "#define NO_BOMBING_MACROS 1\n")
	FILE(APPEND  ${CONFIG_H_FILE} "/*Define to not perform magic number checking */\n")
	FILE(APPEND  ${CONFIG_H_FILE} "#define NO_MAGIC_CHECKING 1\n")
	FILE(APPEND  ${CONFIG_H_FILE} "/*Define to not check for divide by zero during ray shots */\n")
	FILE(APPEND  ${CONFIG_H_FILE} "#define NO_BADRAY_CHECKING 1\n")
	FILE(APPEND  ${CONFIG_H_FILE} "/*Define to not provide run-time debug facilities via rt_g.debug */\n")
	FILE(APPEND  ${CONFIG_H_FILE} "#define NO_DEBUG_CHECKING 1\n")
ENDIF(NOT BRLCAD-ENABLE_RUNTIME_DEBUG)

# Enable/disable strict compiler settings - these are limited to libraries that
# specifically inform the BRLCAD_ADDLIB macro they can be built with STRICT flags.
OPTION(BRLCAD-ENABLE_STRICT "Use strict compiler settings on libraries that support them" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_STRICT)
if (BRLCAD-ENABLE_STRICT)
	FILE(APPEND  ${CONFIG_H_FILE} "#define STRICT_FLAGS 1\n")
endif (BRLCAD-ENABLE_STRICT)

# Build with O3 compiler optimization
OPTION(BRLCAD-ENABLE_OPTIMIZED_BUILD "Use optimized compiler settings" OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_OPTIMIZED_BUILD)

# Build with compiler warning flags 
OPTION(BRLCAD-ENABLE_COMPILER_WARNINGS "Use compiler warning flags" OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_COMPILER_WARNINGS)

# Build with debug symbols
OPTION(BRLCAD-ENABLE_DEBUG "Use debugging compiler settings" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_DEBUG)

# Build with profiling support
OPTION(BRLCAD-ENABLE_PROFILING "Build with profiling support" OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_PROFILING)

# Build with dtrace support
OPTION(BRLCAD-ENABLE_DTRACE "Build with dtrace support" OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_DTRACE)
IF(BRLCAD-ENABLE_DTRACE)
	BRLCAD_INCLUDE_FILE(sys/sdt.h HAVE_SYS_SDT_H)
	IF(NOT HAVE_SYS_SDT_H)
		SET(BRLCAD-ENABLE_DTRACE OFF)
	ENDIF(NOT HAVE_SYS_SDT_H)
ENDIF(BRLCAD-ENABLE_DTRACE)

# Enable/disable 64-bit build settings. This is autodetcted based on
# the size of the void pointer - don't override this setting unless
# you know what you are doing.
IF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
	OPTION(BRLCAD-ENABLE_64BIT "64 bit void pointer" ON)
ELSE(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
	math(EXPR platform_bit_type "${CMAKE_SIZEOF_VOID_P} * 8")
	OPTION(BRLCAD-ENABLE_64BIT "${platform_bit_type} bit void pointer" OFF)
ENDIF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
FILE(APPEND  ${CONFIG_H_FILE} "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")
MARK_AS_ADVANCED(BRLCAD-ENABLE_64BIT)

# Take advantage of parallel processors if available - highly recommended
OPTION(BRLCAD-ENABLE_SMP "Enable SMP architecture parallel computation support" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_SMP)
if (BRLCAD-ENABLE_SMP)
	FILE(APPEND  ${CONFIG_H_FILE} "#define PARALLEL 1\n")
endif (BRLCAD-ENABLE_SMP)

# *******************************************************************
# ***            Check for Programs - Stage 2 of 9                ***
# *******************************************************************

# A variety of tools, such as the benchmark utilities, need
# a Bourne shell and other commands - check for them.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindShellDeps.cmake)

# CMake can detect flex and bison, but apparently not generic lex/yacc.
# Solution is to make our own generic lex/yacc Find routines that look
# first for flex/bison, but will take whatever's available.  Since the
# BRL-CAD project isn't supposed to rely on flex/bison specific features
# anyway, there shouldn't be a problem.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindLEX.cmake)
INCLUDE(${BRLCAD_CMAKE_DIR}/FindYACC.cmake)

# If building extra docs, need to look for XSLTPROC and possibly
# Apache FOP.  Based on what is found, turn on/off Docbook settings
if (BRLCAD-BUILD_EXTRADOCS)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindXSLT.cmake)
	IF(NOT XSLTPROC_EXEC)
		MESSAGE("Extra doc build requested, but xsltproc not found - disabling.")
		SET(BRLCAD-BUILD_EXTRADOCS OFF)
	ENDIF(NOT XSLTPROC_EXEC)
	IF(BRLCAD-BUILD_EXTRADOCS_PDF)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindFOP.cmake)
	IF(NOT APACHE_FOP)
		MESSAGE("Extra doc PDF build requested, but Apache FOP not found - disabling.")
		SET(BRLCAD-BUILD_EXTRADOCS_PDF OFF)
		SET(BRLCAD-BUILD_EXTRADOCS_PDF_MAN OFF)
	ENDIF(NOT APACHE_FOP)

	ENDIF(BRLCAD-BUILD_EXTRADOCS_PDF)
endif (BRLCAD-BUILD_EXTRADOCS)

# If using dtrace, we will need to find it
IF(BRLCAD-ENABLE_DTRACE)
	FIND_PROGRAM(DTRACE_EXEC NAMES dtrace DOC "path to dtrace executable")
ENDIF(BRLCAD-ENABLE_DTRACE)

# *******************************************************************
# ***            Check for Libraries - Stage 3 of 9               ***
# *******************************************************************

# While the primary purpose of this section is to identify libraries,
# some of the headers we are looking for are associated with the
# libraries checked here.  In those cases, we will hand the header 
# logic here as opposed to separating the header logic from the 
# find_package call.

# TODO - need to make LINKOPT vars for anything here that will
# be referenced in a pkgconfig file

# Load various wrapper macros for checking libraries, headers and 
# functions
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_CheckFunctions.cmake)

# Look for threads (doesn't check for headers)
# The variable CMAKE_THREAD_LIBS_INIT is the one of interest
# when writing target_link_libraries lists.
find_package(Threads)
BRLCAD_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)

# Check for the daemon function in -lbsd and/or -lc for adrt
BRLCAD_CHECK_LIBRARY(BSD bsd daemon)
BRLCAD_CHECK_LIBRARY(BSD c daemon)

# Check for CoreFoundation, Carbon and Cocoa on Apple
IF(APPLE)
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindCarbon.cmake)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindCocoa.cmake)
	BRLCAD_INCLUDE_FILE(Carbon/Carbon.h HAVE_CARBON_CARBON_H) 
ENDIF(APPLE)

# crypt library (needed on some platforms for enigma)
find_library(CRYPT_LIBRARY NAMES crypt)

# Find Java and JNI - conditionalize the include settings on 
# the success of the find_package calls.
find_package(Java)
find_package(JNI)
IF(JNI_FOUND AND JAVA_FOUND)
	BRLCAD_INCLUDE_FILE(JavaVM/jni.h HAVE_JAVAVM_JNI_H)
	BRLCAD_INCLUDE_FILE(jni.h HAVE_JNI_H)
ENDIF(JNI_FOUND AND JAVA_FOUND)
IF(NOT HAVE_JNI_H AND NOT HAVE_JAVAVM_JNI_H)
   SET(BRLCAD-ENABLE_RTSERVER OFF)
   MESSAGE("librtserver build requested, but JNI header not found - disabling")
ENDIF(NOT HAVE_JNI_H AND NOT HAVE_JAVAVM_JNI_H)

# Find X11 if it is enabled
IF(BRLCAD-ENABLE_X11)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindX11.cmake)
   BRLCAD_INCLUDE_FILE(X11/Xlib.h HAVE_X11_XLIB_H)
ENDIF(BRLCAD-ENABLE_X11)

# Locate a curses package, and set the CONFIG_H header var if
# the Find package reports the header present
find_package(Curses)
if (CURSES_HAVE_CURSES_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_CURSES_H 1\n")
endif()

# m library is not needed on Windows
IF(NOT WIN32)
	BRLCAD_CHECK_LIBRARY(M m cos)
ENDIF(NOT WIN32)

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
if(BRLCAD-ENABLE_OPENGL)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
	if (OPENGL_INCLUDE_DIR_GL)
		FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_GL_GL_H 1\n")
	endif (OPENGL_INCLUDE_DIR_GL)
	if (OPENGL_INCLUDE_DIR_GLX)
		FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_GL_GLX_H 1\n")
	endif (OPENGL_INCLUDE_DIR_GLX)
endif (BRLCAD-ENABLE_OPENGL)

# SGI graphics library
BRLCAD_CHECK_LIBRARY(SGIGL gl gconfig)

# Solaris lexer library
BRLCAD_CHECK_LIBRARY(SOLARIS_LEXER l yyless)

# IRIX libgen library
BRLCAD_CHECK_LIBRARY(GEN gen basename)

# *******************************************************************
# ***             Check for Headers - Stage 4 of 9                ***
# *******************************************************************

# make our minimum compliance goal explicit (needs to be tested here
# so header files and types are tested properly)
# CHECK_C_FLAG("std=c99")
# IF(STD_C99_COMPILER_FLAG)
#   SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STD_C99_COMPILER_FLAG}")
#   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STD_C99_COMPILER_FLAG}")
# ENDIF(STD_C99_COMPILER_FLAG)

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

# Tests for standard functionality based on AC_HEADER_* macros  (see
# http://www.gnu.org/software/hello/manual/autoconf/Particular-Headers.html)
# AC_HEADER_STDC
CMAKE_HEADER_STDC()
# AC_HEADER_SYS_WAIT
CMAKE_HEADER_SYS_WAIT()
# dirent.h is POSIX.1, but not present on Windows (grr)
# so we need to check for it
BRLCAD_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)

# Need to compare this with the configure.ac list 
BRLCAD_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
BRLCAD_INCLUDE_FILE(dslib.h HAVE_DSLIB_H)
BRLCAD_INCLUDE_FILE(getopt.h HAVE_GETOPT_H)
BRLCAD_INCLUDE_FILE(gl/device.h HAVE_GL_DEVICE_H)
BRLCAD_INCLUDE_FILE(grp.h HAVE_GRP_H)
BRLCAD_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
BRLCAD_INCLUDE_FILE(io.h HAVE_IO_H)
BRLCAD_INCLUDE_FILE(libgen.h HAVE_LIBGEN_H)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
BRLCAD_INCLUDE_FILE(ncurses.h HAVE_NCURSES_H)
BRLCAD_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
BRLCAD_INCLUDE_FILE(poll.h HAVE_POLL_H)
BRLCAD_INCLUDE_FILE(pwd.h HAVE_PWD_H)
BRLCAD_INCLUDE_FILE(rle.h HAVE_RLE_H)
BRLCAD_INCLUDE_FILE(sched.h HAVE_SCHED_H)
BRLCAD_INCLUDE_FILE(sgtty.h HAVE_SGTTY_H)
BRLCAD_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
BRLCAD_INCLUDE_FILE(sys/file.h HAVE_SYS_FILE_H)
BRLCAD_INCLUDE_FILE(sys/_ioctl.h HAVE_SYS__IOCTL_H)
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
BRLCAD_INCLUDE_FILE(termcap.h HAVE_TERMCAP_H)
BRLCAD_INCLUDE_FILE(terminfo.h HAVE_TERMINFO_H)
BRLCAD_INCLUDE_FILE(termio.h HAVE_TERMIO_H)
BRLCAD_INCLUDE_FILE(termios.h HAVE_TERMIOS_H)
BRLCAD_INCLUDE_FILE(termlib.h HAVE_TERMLIB_H)
BRLCAD_INCLUDE_FILE(term.h HAVE_TERM_H)
BRLCAD_INCLUDE_FILE(tinfo.h HAVE_TINFO_H)
BRLCAD_INCLUDE_FILE(ulocks.h HAVE_ULOCKS_H)
BRLCAD_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
BRLCAD_INCLUDE_FILE(winsock.h HAVE_WINSOCK_H)

# C++
BRLCAD_INCLUDE_FILE_CXX(limits HAVE_LIMITS)
BRLCAD_INCLUDE_FILE_CXX(istream HAVE_ISTREAM)

# Other special-case tests that need custom macros
CHECK_BASENAME()
CHECK_DIRNAME()

# *******************************************************************
# ***         Check for Types/Structures - Stage 5 of 9           ***
# *******************************************************************

BRLCAD_STRUCT_MEMBER("struct stat" st_blksize sys/stat.h STRUCT_STAT_ST_BLKSIZE)
BRLCAD_STRUCT_MEMBER("struct stat" st_blocks sys/stat.h STRUCT_STAT_ST_BLOCKS)
BRLCAD_STRUCT_MEMBER("struct stat" st_rdev sys/stat.h STRUCT_STAT_ST_RDEV)

# socklen_t
BRLCAD_TYPE_SIZE("socklen_t" SOCKLEN_T "sys/types.h")
IF(NOT SOCKLEN_T)
	BRLCAD_TYPE_SIZE("socklen_t" SOCKLEN_T "sys/socket.h")
ENDIF(NOT SOCKLEN_T)

BRLCAD_TYPE_SIZE("int" INT "")
BRLCAD_TYPE_SIZE("long" LONG "")
BRLCAD_TYPE_SIZE("long long" LONG_LONG "")
BRLCAD_TYPE_SIZE("ssize_t" SSIZE "")
BRLCAD_TYPE_SIZE("ssize_t" SSIZE "")
BRLCAD_TYPE_SIZE("uint64_t" UINT64 "")
BRLCAD_TYPE_SIZE("uintptr_t" UINTPTR "")
BRLCAD_TYPE_SIZE("sig_t" SIG "signal.h")



# *******************************************************************
# ***       Check for Compile Characteristics - Stage 6 of 9      ***
# *******************************************************************

INCLUDE(${BRLCAD_CMAKE_DIR}/CompilerFlags.cmake)

# *******************************************************************
# ***             Check for Functions - Stage 7 of 9              ***
# *******************************************************************

# ALLOCA test - based on AC_FUNC_ALLOCA
CMAKE_ALLOCA()

BRLCAD_FUNCTION_EXISTS(XQueryExtension HAVE_XQUERYEXTENSION)
BRLCAD_FUNCTION_EXISTS(alarm HAVE_ALARM)
BRLCAD_FUNCTION_EXISTS(dlopen HAVE_DLOPEN)
BRLCAD_FUNCTION_EXISTS(drand48 HAVE_DRAND48)
BRLCAD_FUNCTION_EXISTS(fchmod HAVE_FCHMOD)
BRLCAD_FUNCTION_EXISTS(fsync HAVE_FSYNC)
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
BRLCAD_FUNCTION_EXISTS(writev HAVE_WRITEV)


# *******************************************************************
# ***          Check for System Services - Stage 8 of 9           ***
# *******************************************************************



# This is the top level control property for handling library building 
set(BRLCAD-BUILD_SRC_OTHER "Auto" CACHE STRING
	"Set build type" FORCE)
set_property(CACHE BRLCAD-BUILD_SRC_OTHER PROPERTY STRINGS
	"Yes" "No" "Auto")



# Virtually all of the BRL-CAD code needs the BRL-CAD include directories
# and a couple of standard defines - set those up in variables now - the
# idea being that if any other system-wide defines or include are needed
# they can just be added here.
SET(BRLCAD_STD_DEFS "-DHAVE_CONFIG_H;-DBRLCADBUILD=1")
SET(BRLCAD_STD_INCLUDES "${BRLCAD_BINARY_DIR}/include;${BRLCAD_SOURCE_DIR}/include")

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(db)
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(sh)
ADD_SUBDIRECTORY(misc)
ADD_SUBDIRECTORY(pix)
ADD_SUBDIRECTORY(bench)
ADD_SUBDIRECTORY(regress)

# Define the source and build directories in the config header for tcl auto_path setting
# The install directory is already avaiable in C using bu_brlcad_root
FILE(APPEND  ${CONFIG_H_FILE} "#define BUILD_BINARY_DIR \"${CMAKE_BINARY_DIR}\"\n")
FILE(APPEND  ${CONFIG_H_FILE} "#define BUILD_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\"\n")

# Now that all the tests are done, configure the brlcad_config.h file:
FILE(APPEND ${CONFIG_H_FILE} "#endif /* __CONFIG_H__ */\n")
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)


# *******************************************************************
# ***              Summarize Results - Stage 9 of 9               ***
# *******************************************************************

# Now that everything is configured, print a summary of the build settings.  This is a bit complicated, but
# the basic idea is to print labels, "smart" spacers around those labels to line everything up, and the
# value of the appropriate build variable.  The convention is to assign the text labeling each value to a
# variable named <variable_name>_LABEL - this allows a loop to iterate over a list of variables and handle
# them all, rather than each line needing its own logic.

OPTION(BRLCAD_PRINT_VERBOSE_SUMMARY "Print summary" ON)
MARK_AS_ADVANCED(BRLCAD_PRINT_VERBOSE_SUMMARY)

if(BRLCAD_PRINT_VERBOSE_SUMMARY)
	MESSAGE("")
	MESSAGE("------ BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build  ------")
	MESSAGE("")

	# Directory settings

	SET(BRLCAD_PREFIX_LABEL "Prefix")
	SET(BRLCAD_INSTALL_BIN_DIR_LABEL "Binaries")
	SET(BRLCAD_INSTALL_MAN_DIR_LABEL "Manual pages")
	SET(BRLCAD_INSTALL_CONF_DIR_LABEL "Configration files")
	SET(BRLCAD_INSTALL_DATA_DIR_LABEL "Data resource files")
	SET(PATHLABELS BRLCAD_PREFIX BRLCAD_INSTALL_BIN_DIR BRLCAD_INSTALL_MAN_DIR BRLCAD_INSTALL_CONF_DIR BRLCAD_INSTALL_DATA_DIR)
	SET(PATHLABELLENGTH 0)
	FOREACH(path_label ${PATHLABELS})
		STRING(LENGTH ${${path_label}_LABEL} CURRENTLENGTH)
		if (${CURRENTLENGTH} GREATER ${PATHLABELLENGTH})
			SET(PATHLABELLENGTH ${CURRENTLENGTH})
		endif()
	ENDFOREACH()
	FOREACH(path_label ${PATHLABELS})
		SET(CURRENT_LABEL ${${path_label}_LABEL})
		STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
		WHILE(${PATHLABELLENGTH} GREATER ${CURRENTLENGTH})
			SET(CURRENT_LABEL " ${CURRENT_LABEL}")
			STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
		ENDWHILE(${PATHLABELLENGTH} GREATER ${CURRENTLENGTH})
		MESSAGE("${CURRENT_LABEL}: ${${path_label}}")
	ENDFOREACH() 
	MESSAGE("")
	IF(CMAKE_BUILD_TYPE MATCHES "^Basic$")
		SET(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS}")
		SET(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	endif()
	IF(CMAKE_BUILD_TYPE MATCHES "^Debug$")
		SET(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
		SET(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
	endif()
	IF(CMAKE_BUILD_TYPE MATCHES "^Release$")
		SET(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
		SET(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
	endif()

	# Compiler settings - in this case there are additional "special case" flags that get
	# added depending on build type - roll all flags into individual variables before printing

	SET(CMAKE_C_COMPILER_LABEL "CC")
	SET(CMAKE_CXX_COMPILER_LABEL "CXX")
	SET(CMAKE_ALL_C_FLAGS_LABEL "CFLAGS")
	SET(CMAKE_ALL_CXX_FLAGS_LABEL "CXXFLAGS")
	SET(CMAKE_CPPFLAGS_LABEL "CPPFLAGS")
	SET(CMAKE_LDFLAGS_LABEL "LDFLAGS")
	SET(SETTINGLABELS CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_ALL_C_FLAGS CMAKE_ALL_CXX_FLAGS CMAKE_CPPFLAGS CMAKE_LDFLAGS)
	SET(SETTINGLABELLENGTH 0)
	FOREACH(setting_label ${SETTINGLABELS})
		STRING(LENGTH ${${setting_label}_LABEL} CURRENTLENGTH)
		if (${CURRENTLENGTH} GREATER ${SETTINGLABELLENGTH})
			SET(SETTINGLABELLENGTH ${CURRENTLENGTH})
		endif()
	ENDFOREACH()
	FOREACH(setting_label ${SETTINGLABELS})
		SET(CURRENT_LABEL ${${setting_label}_LABEL})
		STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
		WHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
			SET(CURRENT_LABEL "${CURRENT_LABEL} ")
			STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
		ENDWHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
		MESSAGE("${CURRENT_LABEL} = ${${setting_label}}")
	ENDFOREACH() 
	MESSAGE("")

	# Build options
	SET(BRLCAD_BUILD_LOCAL_TCL_LABEL "Build Tcl ")
	SET(BRLCAD_BUILD_LOCAL_TK_LABEL "Build Tk ")
	SET(BRLCAD_BUILD_LOCAL_INCRTCL_LABEL "Build Itcl/Itk ")
	SET(BRLCAD_BUILD_LOCAL_IWIDGETS_LABEL "Build Iwidgets ")
	SET(BRLCAD_BUILD_LOCAL_TKHTML_LABEL "Build Tkhtml ")
	SET(BRLCAD_BUILD_LOCAL_TKPNG_LABEL "Build tkpng ")
	SET(BRLCAD_BUILD_LOCAL_TKTABLE_LABEL "Build Tktable ")
	SET(BRLCAD_BUILD_LOCAL_PNG_LABEL "Build libpng ")
	SET(BRLCAD_BUILD_LOCAL_REGEX_LABEL "Build libregex ")
	SET(BRLCAD_BUILD_LOCAL_ZLIB_LABEL "Build zlib ")
	SET(BRLCAD_BUILD_LOCAL_TERMLIB_LABEL "Build termlib ")
	SET(BRLCAD_BUILD_LOCAL_UTAHRLE_LABEL "Build Utah Raster Toolkit ")
	SET(BRLCAD_BUILD_LOCAL_OPENNURBS_LABEL "Build openNURBS ")
	SET(BRLCAD_BUILD_LOCAL_SCL_LABEL "Build NIST STEP Class Libraries ")
	SET(BRLCAD-ENABLE_X11_LABEL "X11 support (optional) ")
	SET(BRLCAD-ENABLE_OPENGL_LABEL "OpenGL support (optional) ")
	SET(BRLCAD-ENABLE_RTSERVER_LABEL "librtserver JDK support (optional) ")
	SET(BRLCAD-ENABLE_RUNTIMEDEBUGGING_LABEL "Enable run-time debugging (optional) ")
	if (BRLCAD-ENABLE_64BIT)
		set(BRLCAD_ARCH_BITSETTING "ON")
	else (BRLCAD-ENABLE_64BIT)
		math(EXPR platform_bit_type "${CMAKE_SIZEOF_VOID_P} * 8")
		set(BRLCAD_ARCH_BITSETTING "OFF (${platform_bit_type} bit)")
	endif (BRLCAD-ENABLE_64BIT)
	SET(BRLCAD_ARCH_BITSETTING_LABEL "Build 64-bit release ")
	SET(BRLCAD-ENABLE_OPTIMIZED_LABEL "Build optimized release ")
	SET(BRLCAD-ENABLE_DEBUG_LABEL "Build debug release ")
	SET(BRLCAD-ENABLE_PROFILE_LABEL "Build profile release ")
	SET(BRLCAD-ENABLE_SMP_LABEL "Build SMP-capable release ")
	SET(BRLCAD-ENABLE_STATICLIBS_LABEL "Build static libraries ")
	SET(BRLCAD-ENABLE_DYNAMICLIBS_LABEL "Build dynamic libraries ")
	SET(BRLCAD-ENABLE_VERBOSEWARN_LABEL "Print verbose compilation warnings ")
	SET(BRLCAD-ENABLE_VERBOSEPROGRESS_LABEL "Print verbose compilation progress ")
	SET(BRLCAD-ENABLE_BENCHMARK_ONLY_LABEL "Only build benchmark suite ")
	SET(BRLCAD-ENABLE_RTSERVER_ONLY_LABEL "Only build librtserver ")
	SET(BRLCAD-INSTALL_EXAMPLE_GEOMETRY_LABEL "Install example geometry models ")
	if (BRLCAD-BUILD_EXTRADOCS)
		if (BRLCAD-BUILD_EXTRADOCS_PDF)
			set(BRLCAD-BUILD_DOCBUILD_MSG "ON (man/html/pdf)")
		else(BRLCAD-BUILD_EXTRADOCS_PDF)
			set(BRLCAD-BUILD_DOCBUILD_MSG "ON (man/html only)")
		endif(BRLCAD-BUILD_EXTRADOCS_PDF)
	else (BRLCAD-BUILD_EXTRADOCS)
		set(BRLCAD-BUILD_DOCBUILD_MSG "OFF")
	endif (BRLCAD-BUILD_EXTRADOCS)
	SET(BRLCAD-BUILD_DOCBUILD_MSG_LABEL "Build extra docs ")

	SET(SETTINGLIST1 BRLCAD_BUILD_LOCAL_TCL BRLCAD_BUILD_LOCAL_TK BRLCAD_BUILD_LOCAL_INCRTCL BRLCAD_BUILD_LOCAL_IWIDGETS BRLCAD_BUILD_LOCAL_TKHTML BRLCAD_BUILD_LOCAL_TKPNG BRLCAD_BUILD_LOCAL_TKTABLE BRLCAD_BUILD_LOCAL_PNG BRLCAD_BUILD_LOCAL_REGEX BRLCAD_BUILD_LOCAL_ZLIB BRLCAD_BUILD_LOCAL_TERMLIB BRLCAD_BUILD_LOCAL_UTAHRLE BRLCAD_BUILD_LOCAL_OPENNURBS BRLCAD_BUILD_LOCAL_SCL)
	SET(SETTINGLIST2 BRLCAD-ENABLE_X11 BRLCAD-ENABLE_OPENGL BRLCAD-ENABLE_RTSERVER BRLCAD-ENABLE_RUNTIMEDEBUGGING )
	SET(SETTINGLIST3 BRLCAD_ARCH_BITSETTING BRLCAD-ENABLE_OPTIMIZED BRLCAD-ENABLE_DEBUG BRLCAD-ENABLE_PROFILE BRLCAD-ENABLE_SMP BRLCAD-ENABLE_STATICLIBS BRLCAD-ENABLE_DYNAMICLIBS BRLCAD-ENABLE_VERBOSEWARN BRLCAD-ENABLE_VERBOSEPROGRESS BRLCAD-ENABLE_BENCHMARK_ONLY BRLCAD-ENABLE_RTSERVER_ONLY BRLCAD-INSTALL_EXAMPLE_GEOMETRY BRLCAD-BUILD_DOCBUILD_MSG)
	SET(ARRAYLIST SETTINGLIST1 SETTINGLIST2 SETTINGLIST3)
	SET(SETTINGLABELLENGTH 0)
	FOREACH(setting_array ${ARRAYLIST})
		FOREACH(setting_label ${${setting_array}})
			STRING(LENGTH ${${setting_label}_LABEL} CURRENTLENGTH)
			if (${CURRENTLENGTH} GREATER ${SETTINGLABELLENGTH})
				SET(SETTINGLABELLENGTH ${CURRENTLENGTH})
			endif()
		ENDFOREACH()
	ENDFOREACH()
	FOREACH(setting_array ${ARRAYLIST})
		FOREACH(setting_label ${${setting_array}})
			SET(CURRENT_LABEL ${${setting_label}_LABEL})
			STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
			WHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
				SET(CURRENT_LABEL "${CURRENT_LABEL}.")
				STRING(LENGTH ${CURRENT_LABEL} CURRENTLENGTH)
			ENDWHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
			MESSAGE("${CURRENT_LABEL}..: ${${setting_label}}")
		ENDFOREACH() 
		MESSAGE("")
	ENDFOREACH() 

endif(BRLCAD_PRINT_VERBOSE_SUMMARY)

# Set up rules to print a timestamp string during build
ADD_EXECUTABLE(printtimestamp ${BRLCAD_CMAKE_DIR}/test_srcs/print_timestamp.c)
ADD_CUSTOM_TARGET(timestamp ALL	COMMAND printtimestamp)

# Some of the BRL-CAD targets need to depend on the local
# tcl/tk targets if they are present
SET(TCL_DEPLIBS "libbu;libsysv;librt")
if(BRLCAD_BUILD_LOCAL_TCL)
	FOREACH(target ${TCL_DEPLIBS})
		ADD_DEPENDENCIES(${target} tcl)
	ENDFOREACH(target ${TCL_DEPLIBS})
endif(BRLCAD_BUILD_LOCAL_TCL)
if(BRLCAD_BUILD_LOCAL_TK)
	FOREACH(target ${TCL_DEPLIBS})
		ADD_DEPENDENCIES(${target} tk)
	ENDFOREACH(target ${TCL_DEPLIBS})
endif(BRLCAD_BUILD_LOCAL_TK)

# Set up the noprod rule.  Currently this is an "all or nothing"
# command that will clear ALL products for lib or exec targets,
# not just those in a particular subdirectory.
STRING(REGEX REPLACE ";" "\\\;" LIBRARY_TARGET_FILES "${CMAKE_LIBRARY_TARGET_FILE_LIST}")
STRING(REGEX REPLACE ";" "\\\;" EXEC_TARGET_FILES "${CMAKE_EXEC_TARGET_FILE_LIST}")
SET(noprod_file "
SET(CMAKE_LIBRARY_TARGET_FILE_LIST \"${LIBRARY_TARGET_FILES}\")
    MESSAGE(\"current dir: \${CMAKE_CURRENT_BINARY_DIR}\\n\")
FOREACH(file \${CMAKE_LIBRARY_TARGET_FILE_LIST})
  IF(EXISTS \${file})
    MESSAGE(\"removing \${file}\\n\")
    EXEC_PROGRAM(
      ${CMAKE_COMMAND} ARGS \"-E remove \${file}\"
      OUTPUT_VARIABLE rm_out
      RETURN_VALUE rm_retval
      )
    IF(NOT \"\${rm_retval}\" STREQUAL 0)
      MESSAGE(FATAL_ERROR \"Problem when removing \${file}\")
    ENDIF(NOT \"\${rm_retval}\" STREQUAL 0)
  ENDIF(EXISTS \${file})
ENDFOREACH(file \${CMAKE_LIBRARY_TARGET_FILE_LIST})

SET(CMAKE_EXEC_TARGET_FILE_LIST \"${EXEC_TARGET_FILES}\")
FOREACH(file \${CMAKE_EXEC_TARGET_FILE_LIST})
  IF(EXISTS \${file})
    MESSAGE(\"removing \${file}\\n\")
    EXEC_PROGRAM(
      ${CMAKE_COMMAND} ARGS \"-E remove \${file}\"
      OUTPUT_VARIABLE rm_out
      RETURN_VALUE rm_retval
      )
    IF(NOT \"\${rm_retval}\" STREQUAL 0)
      MESSAGE(FATAL_ERROR \"Problem when removing \${file}\")
    ENDIF(NOT \"\${rm_retval}\" STREQUAL 0)
  ENDIF(EXISTS \${file})
ENDFOREACH(file \${CMAKE_EXEC_TARGET_FILE_LIST})
")
FILE(WRITE ${CMAKE_BINARY_DIR}/cmake_noprod.cmake ${noprod_file})
ADD_CUSTOM_TARGET(noprod
  "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_noprod.cmake")


# We want the timestamp to come first, so make ALL library,
# exec and external targets depend on timestamp (except for
# the exec target printtimestamp, which is needed by timestamp)
LIST(REMOVE_DUPLICATES CMAKE_LIBRARY_TARGET_LIST)
LIST(REMOVE_DUPLICATES CMAKE_EXEC_TARGET_LIST)
LIST(REMOVE_DUPLICATES CMAKE_CUSTOM_TARGET_LIST)
IF(CMAKE_EXTERNAL_TARGET_LIST)
   LIST(REMOVE_DUPLICATES CMAKE_EXTERNAL_TARGET_LIST)
ENDIF(CMAKE_EXTERNAL_TARGET_LIST)
FOREACH(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
	get_target_property(target_location ${libtarget} LOCATION)
	if(target_location)
		ADD_DEPENDENCIES(${libtarget} timestamp)
	endif(target_location)
ENDFOREACH(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
FOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
	get_target_property(target_location ${exectarget} LOCATION)
	if(target_location)
		IF(NOT ${exectarget} MATCHES "printtimestamp")
			ADD_DEPENDENCIES(${exectarget} timestamp)
		ENDIF(NOT ${exectarget} MATCHES "printtimestamp")
	endif(target_location)
ENDFOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
FOREACH(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
		IF(NOT ${custtarget} MATCHES "timestamp")
			ADD_DEPENDENCIES(${custtarget} timestamp)
		ENDIF(NOT ${custtarget} MATCHES "timestamp")
ENDFOREACH(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
FOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
	get_target_property(target_confcmd ${externaltarget} _EP_CONFIGURE_COMMAND)
	if(target_confcmd)
		ADD_DEPENDENCIES(${externaltarget} timestamp)
	endif(target_confcmd)
ENDFOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})


# Generate the include/conf files

# HOST 
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_hostname.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c)
IF(NOT MSVC)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c
	OUTPUT_VARIABLE COMPILEMESSAGES)
ELSE(NOT MSVC)
SET(CMAKE_REQUIRED_LIBRARIES ws2_32.lib)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c
	OUTPUT_VARIABLE COMPILEMESSAGES
	CMAKE_FLAGS "-DLINK_LIBRARIES:STRING=ws2_32.lib")
ENDIF(NOT MSVC)
FILE(APPEND ${BRLCAD_BINARY_DIR}/include/conf/HOST "\n") 

# USER
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_username.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c
	OUTPUT_VARIABLE COMPILEMESSAGES)
FILE(APPEND ${BRLCAD_BINARY_DIR}/include/conf/USER "\n") 

# PATH
FILE(WRITE ${BRLCAD_BINARY_DIR}/include/conf/PATH "\"${BRLCAD_PREFIX}\"") 
FILE(APPEND ${BRLCAD_BINARY_DIR}/include/conf/PATH "\n") 

# COUNT - force CMake to look every time in case of manual removal of COUNT file
set(countFile "countFile-NOTFOUND" CACHE STRING "Is there a count file?" FORCE)
find_file(countFile COUNT PATHS ${BRLCAD_BINARY_DIR}/include/conf)
if(countFile MATCHES "^countFile-NOTFOUND$")
	set(buildCounter 0)
else()
	file(READ ${countFile} buildCounter)
	math(EXPR buildCounter "${buildCounter} + 1")
endif()
file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/COUNT "${buildCounter}")
FILE(APPEND ${BRLCAD_BINARY_DIR}/include/conf/COUNT "\n") 

# DATE
file(READ ${CONFIG_TIMESTAMP_FILE} DATESTAMP)
STRING(STRIP ${DATESTAMP} DATESTAMP)
file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/DATE "\"${DATESTAMP}\"")
FILE(APPEND ${BRLCAD_BINARY_DIR}/include/conf/DATE "\n") 

# Rules for the toplevel documentation files
SET(toplevel_DOCFILES
  AUTHORS
  COPYING
  HACKING
  INSTALL
  NEWS
  README
)
INSTALL(FILES ${toplevel_DOCFILES} DESTINATION ${BRLCAD_INSTALL_DATA_DIR})

#Done with everything else - do the configure time delta
SET(DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_END")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/timedelta_end.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_end.c
	OUTPUT_VARIABLE COMPILEMESSAGES)
FILE(READ ${DELTA_END} CONFIG_TIME_MSG)
STRING(STRIP ${CONFIG_TIME_MSG} CONFIG_TIME_MSG)
SET(CONFIG_TIME_MSG_LABEL "Elapsed configuration time")
STRING(LENGTH ${CONFIG_TIME_MSG_LABEL} CURRENTLENGTH)
WHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
	SET(CONFIG_TIME_MSG_LABEL "${CONFIG_TIME_MSG_LABEL}.")
	STRING(LENGTH ${CONFIG_TIME_MSG_LABEL} CURRENTLENGTH)
ENDWHILE(${SETTINGLABELLENGTH} GREATER ${CURRENTLENGTH})
MESSAGE("${CONFIG_TIME_MSG_LABEL}..: ${CONFIG_TIME_MSG}")
