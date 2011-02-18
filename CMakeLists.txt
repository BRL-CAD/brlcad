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
	CMAKE_POLICY(SET CMP0007 OLD)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(BRLCAD)

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
# directory.  To work around this, alter the CMAKE_SYSTEM_PREFIX_PATH
# variable identified by Maik Beckmann:
#
# http://www.cmake.org/pipermail/cmake/2010-October/040292.html
#
# The complication is if we are installing to a "legitimate" system
# search path - i.e. our CMAKE_INSTALL_PREFIX value is standard
# enough that it is a legitimate search target for find_package.
# In this case, we can't exclude accidental hits on our libraries
# without also excluding legitimate find_package results.  So the net
# results are:
#
# 1.  If you are planning to install to a system directory (typically
#     a bad idea but the settings are legal) clean out the old system
#     first or accept that the old libraries will be found and used.
#
# 2.  For more custom paths, the logic below will strip out the value
#     of CMAKE_INSTALL_PREFIX to avoid its use in find_package searches

IF(CMAKE_INSTALL_PREFIX AND BRLCAD_PREFIX)
	IF(NOT ${BRLCAD_PREFIX} MATCHES "^/usr$" AND NOT ${BRLCAD_PREFIX} MATCHES "^/usr/local$")
		LIST(FIND CMAKE_SYSTEM_PREFIX_PATH ${BRLCAD_PREFIX} BRLCAD_PREFIX_IN_SEARCH)
		IF(BRLCAD_PREFIX_IN_SEARCH)
			LIST(REMOVE_AT CMAKE_SYSTEM_PREFIX_PATH ${BRLCAD_PREFIX_IN_SEARCH})
			SET(CMAKE_SYSTEM_PREFIX_PATH ${CMAKE_SYSTEM_PREFIX_PATH} CACHE STRING "Tweaked to avoid inconsistent search results")
		ENDIF(BRLCAD_PREFIX_IN_SEARCH)
	ENDIF(NOT ${BRLCAD_PREFIX} MATCHES "^/usr$" AND NOT ${BRLCAD_PREFIX} MATCHES "^/usr/local$")
ELSE(CMAKE_INSTALL_PREFIX AND BRLCAD_PREFIX)
	SET(CMAKE_SYSTEM_PREFIX_PATH ${CMAKE_SYSTEM_PREFIX_PATH} CACHE STRING "Tweaked to avoid inconsistent search results")
ENDIF(CMAKE_INSTALL_PREFIX AND BRLCAD_PREFIX)
MARK_AS_ADVANCED(CMAKE_SYSTEM_PREFIX_PATH)

#---------------------------------------------------------------------
# By default (as of version 2.8.2) CMake does not provide access to 
# global lists of executable and library targets.  This is useful
# in a number of situations related to formulating custom rules and
# target dependency management.  To avoid the necessity of replacing
# add_library and add_executable calls with custom macros, override
# the function names and call the _add_* functions to access the CMake
# functionality previously available under the add_* functions. See
# http://www.cmake.org/pipermail/cmake/2010-September/039388.html

# A further compilcation is that functions have local variable scope,
# hence the use of properties to allow access to directory-specific
# and global information scopes.

# The BRL-CAD build generates noprod files that can be
# manually run to simulate the workings of make noprod in that directory:
#
# cmake -P ./noprod
#
# This will remove just the final output files from all exec and
# library targets in that directory.  It is not practical to have
# 'make noprod' targets in each directory in the current CMake
# system, so there is one toplevel 'noprod' target and in subdirectories
# the above command must be used.

MACRO(CMAKEFILES)
	FOREACH(ITEM ${ARGN})
		IF(NOT ${ITEM} MATCHES "^SHARED$"
				AND NOT ${ITEM} MATCHES "^STATIC$")
			GET_FILENAME_COMPONENT(ITEM_PATH ${ITEM} PATH)
			IF(ITEM_PATH)
				GET_FILENAME_COMPONENT(ITEM_ABS_PATH ${ITEM_PATH} ABSOLUTE)
				IF(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
					FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake	"\\${ITEM}")
					FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmakepaths.cmake	"\\${ITEM_PATH}")
				ENDIF(NOT ${ITEM_PATH} MATCHES "^${ITEM_ABS_PATH}$")
			ELSE(ITEM_PATH)
				FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake	"\\${ITEM}")
			ENDIF(ITEM_PATH)
		ENDIF()
	ENDFOREACH(ITEM ${ARGN})
ENDMACRO(CMAKEFILES FILESLIST)


define_property(GLOBAL PROPERTY CHILD_LIST BRIEF_DOCS "child list" FULL_DOCS "tmp child target list")

define_property(GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST BRIEF_DOCS "libtarget list" FULL_DOCS "Library target list")
function(add_library name)
	get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
	_add_library(${name} ${ARGN})
	SET(LIBFILESLIST ${ARGN})
	CMAKEFILES(${ARGN})
	IF(${name} MATCHES "^lib*")
		set_target_properties(${name} PROPERTIES PREFIX "")
	ENDIF(${name} MATCHES "^lib*")
	set_property(GLOBAL APPEND PROPERTY CMAKE_LIBRARY_TARGET_LIST ${name})
	set_property(GLOBAL APPEND PROPERTY CHILD_LIST ${name})
endfunction(add_library)

define_property(GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST BRIEF_DOCS "exec target list" FULL_DOCS "Executable target list")
function(add_executable name)
	get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
   _add_executable(${name} ${ARGN})
	CMAKEFILES(${ARGN})
	set_property(GLOBAL APPEND PROPERTY CMAKE_EXEC_TARGET_LIST ${name})
	set_property(GLOBAL APPEND PROPERTY CHILD_LIST ${name})
endfunction(add_executable)

define_property(GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST BRIEF_DOCS "custom target list" FULL_DOCS "Custom target list")
function(add_custom_target name)
	get_property(CMAKE_CUSTOM_TARGET_LIST GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST)
   _add_custom_target(${name} ${ARGN})
	set_property(GLOBAL APPEND PROPERTY CMAKE_CUSTOM_TARGET_LIST ${name})
endfunction(add_custom_target)
MARK_AS_ADVANCED(CMAKE_LIBRARY_TARGET_LIST)
MARK_AS_ADVANCED(CMAKE_EXEC_TARGET_LIST)
MARK_AS_ADVANCED(CMAKE_CUSTOM_TARGET_LIST)

define_property(DIRECTORY PROPERTY DIR_TARGET_LIST BRIEF_DOCS "dir list" FULL_DOCS "Directory target list")
define_property(GLOBAL PROPERTY DISTCHECK_DIRS BRIEF_DOCS "check during distcheck" FULL_DOCS "List of directories to check during make distcheck")
set_property(GLOBAL PROPERTY DISTCHECK_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
function(add_subdirectory name)
	set_property(GLOBAL PROPERTY CHILD_LIST "")
	FILE(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${name}/cmakefiles.cmake)
	FILE(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${name}/cmakepaths.cmake)
	set_property(GLOBAL APPEND PROPERTY DISTCHECK_DIRS	"\\${CMAKE_CURRENT_SOURCE_DIR}/${name}")
	_add_subdirectory(${name} ${ARGN})
	GET_FILENAME_COMPONENT(ITEM_NAME ${name} NAME)
	FILE(APPEND ${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake	"\\${ITEM_NAME}")
	get_property(target_list GLOBAL PROPERTY CHILD_LIST)
	set_property(DIRECTORY ${name} APPEND PROPERTY DIR_TARGET_LIST	${target_list})
	set_property(DIRECTORY APPEND PROPERTY DIR_TARGET_LIST ${target_list})
	get_property(target_list DIRECTORY ${name} PROPERTY DIR_TARGET_LIST)
		IF(target_list)
		SET(CMAKE_TARGET_FILE_LIST "")
		FOREACH(target ${target_list})
			get_target_property(targetfile ${target} LOCATION)
			get_target_property(targetversion ${target} VERSION)
			get_target_property(targetsoversion ${target} SOVERSION)
			SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}")
			IF(targetsoversion)
				SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}.${targetsoversion}")
			ENDIF(targetsoversion)
			IF(targetversion)
				SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}-${targetversion};${targetfile}.${targetversion}")
			ENDIF(targetversion)
		ENDFOREACH(target ${target_list})

		STRING(REGEX REPLACE ";" "\\\;" TARGET_FILES "${CMAKE_TARGET_FILE_LIST}")
		SET(noprod_file "
SET(CMAKE_TARGET_FILE_LIST \"${TARGET_FILES}\")
FOREACH(file \${CMAKE_TARGET_FILE_LIST})
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
ENDFOREACH(file \${CMAKE_TARGET_FILE_LIST})
"    )
      FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${name}/noprod ${noprod_file})
	   ENDIF(target_list)
	get_property(target_list DIRECTORY PROPERTY DIR_TARGET_LIST)
	get_property(child_list DIRECTORY ${name} PROPERTY DIR_TARGET_LIST)
	IF(target_list)
		LIST(APPEND target_list ${child_list})
		LIST(REMOVE_DUPLICATES target_list)
		set_property(DIRECTORY PROPERTY DIR_TARGET_LIST ${target_list})
	ENDIF(target_list)
	IF(target_list)
		SET(CMAKE_TARGET_FILE_LIST "")
		FOREACH(target ${target_list})
			get_target_property(targetfile ${target} LOCATION)
			get_target_property(targetversion ${target} VERSION)
			get_target_property(targetsoversion ${target} SOVERSION)
			SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}")
			IF(targetsoversion)
				SET(CMAKE_TARGET_FILE_LIST	"${CMAKE_TARGET_FILE_LIST};${targetfile}.${targetsoversion}")
			ENDIF(targetsoversion)
			IF(targetversion)
				SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}-${targetversion};${targetfile}.${targetversion}")
			ENDIF(targetversion)
		ENDFOREACH(target ${target_list})

		STRING(REGEX REPLACE ";" "\\\;" TARGET_FILES "${CMAKE_TARGET_FILE_LIST}")
		SET(noprod_file "
SET(CMAKE_TARGET_FILE_LIST \"${TARGET_FILES}\")
FOREACH(file \${CMAKE_TARGET_FILE_LIST})
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
ENDFOREACH(file \${CMAKE_TARGET_FILE_LIST})
"    )
      FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/noprod ${noprod_file})
   ENDIF(target_list)
endfunction(add_subdirectory)

# The toplevel dir doesn't invoke add_subdirectory on itself,
# so remove any old cmakefiles.cmake file manually
FILE(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/cmakefiles.cmake)
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
# For NFS volumes, to ensure proper file creation.  If UMASK is set
# use it, but it's not a standard setting at the moment.
IF(NOT WIN32)
	IF(NOT UMASK)
		EXEC_PROGRAM(umask ARGS 022 OUTPUT_VARIABLE exec_out)
	ELSE(NOT UMASK)
		EXEC_PROGRAM(umask ARGS ${UMASK} OUTPUT_VARIABLE exec_out)
	ENDIF(NOT UMASK)
ENDIF(NOT WIN32)

#---------------------------------------------------------------------
# We want to check /usr/local by default, so add it 
SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
SET(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} /usr/local/include)

#---------------------------------------------------------------------
# The location in which to install BRLCAD. Need a good Debug location 
# for Windows.  Only do this if CMAKE_INSTALL_PREFIX hasn't been set
# already, to try and allow parent builds (if any) some control.
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	IF(NOT WIN32)
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(CMAKE_INSTALL_PREFIX "/usr/brlcad/rel-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
		ELSEIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			SET(CMAKE_INSTALL_PREFIX "${BRLCAD_SOURCE_DIR}/../brlcad-install")
		ELSE("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(CMAKE_INSTALL_PREFIX "/usr/brlcad")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	ENDIF(NOT WIN32)
	SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "BRL-CAD install prefix" FORCE)
	SET(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
SET(BRLCAD_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "BRL-CAD install prefix")

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
	SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
endif(NOT APPLE)
SET(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to 
# directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable 
# files will be placed after building but prior to install.
IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
	IF(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
	ELSE(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
	ENDIF(WIN32)
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
	IF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
		IF(WIN32)
			SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
		ELSE(WIN32)
			SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
		ENDIF(WIN32)
	ENDIF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
	IF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
		SET("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
	ENDIF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
	IF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
		SET("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
	ENDIF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
ENDFOREACH()

#---------------------------------------------------------------------
# Configure install locations.  It may be that this should be made
# into its own .cmake file and included in each library, to allow
# for more flexibility in using individual libraries in other projects.
MACRO(INSTALL_DIR upper suffix)
	IF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR)
		SET(${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR ${suffix})
		SET(${upper}_DIR ${suffix})
	ENDIF(NOT ${CMAKE_PROJECT_NAME}_INSTALL_${upper}_DIR)
ENDMACRO(INSTALL_DIR upper suffix)

# The location in which to install BRLCAD executables.
INSTALL_DIR(BIN bin)

# The location in which to install BRLCAD header files.
INSTALL_DIR(INCLUDE include)

# The location in which to install BRL-CAD libraries.
IF(NOT WIN32)
   INSTALL_DIR(LIB lib)
ELSE(NOT WIN32)
   INSTALL_DIR(LIB bin)
ENDIF(NOT WIN32)

# The location in which to install BRL-CAD configuration files.
INSTALL_DIR(CONF etc)

# The location in which to install CMake scripts for packaging BRLCAD.
INSTALL_DIR(PACKAGE lib)

# The location in which to install BRL-CAD data files 
INSTALL_DIR(DATA "share/brlcad/${BRLCAD_VERSION}")

# The location in which to install BRL-CAD Manual pages
INSTALL_DIR(MAN ${DATA_DIR}/man)

# The location in which to install BRL-CAD documentation files 
INSTALL_DIR(DOC ${DATA_DIR}/doc)

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
	FILE(APPEND ${CONFIG_H_FILE} "#include \"config_win_cmake.h\"\n")
ENDIF(WIN32)

# Set up some of the define statements for path information and other basics
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE \"brlcad\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"http://brlcad.org\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"BRL-CAD\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"BRL-CAD ${BRLCAD_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"brlcad\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define BRLCAD_DATA \"${CMAKE_INSTALL_PREFIX}/${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define BRLCAD_ROOT \"${CMAKE_INSTALL_PREFIX}\"\n")
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
IF(CMAKE_BUILD_TYPE)
	# If we're doing a debug build, don't build the static libs unless
	# they're specifically asked for (i.e. there is a situation where
	# the developer wants to look into the static libs specifically.
	# Note that this won't turn off libs that are built ONLY as static
	IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND NOT MSVC)
		OPTION(BUILD_STATIC_LIBS "Build static libraries" OFF)
	ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug" AND NOT MSVC)
ELSE(CMAKE_BUILD_TYPE)
	OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)
ENDIF(CMAKE_BUILD_TYPE)

# Turn on librtserver build.  
OPTION(BRLCAD-ENABLE_RTSERVER "Enable the librtserver target." ON)

# Global third party controls - these options enable and disable ALL
# local copies of libraries in src/other.  Forcing all local
# libraries off is not usually recommended unless attempting to
# build packages for a distribution.  If both of these options are
# on the enabling of local packages is the "senior" option and will
# force the system libs option to off.
OPTION(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY "Use only the system's libraries." OFF)
OPTION(BRLCAD-ENABLE_ALL_LOCAL_LIBS "Build all the local copies of libraries." OFF)
IF(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY AND BRLCAD-ENABLE_ALL_LOCAL_LIBS)
	MESSAGE("Both ENABLE and DISABLE all local libs set - using ENABLE")
	SET(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY OFF CACHE BOOL "Use only the system's libraries." FORCE)
ENDIF(BRLCAD-ENABLE_SYSTEM_LIBS_ONLY AND BRLCAD-ENABLE_ALL_LOCAL_LIBS)

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building. Not currently working - needs work in at least Tk and togl
# CMake logic (probably more), and the display manager/framebuffer codes are known to depend
# on either GLX or WGL specifically in their current forms.
OPTION(BRLCAD-ENABLE_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)
MARK_AS_ADVANCED(BRLCAD-ENABLE_AQUA)

# Install example BRL-CAD Geometry Files
OPTION(BRLCAD-INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)

# Enable features requiring X11
if(WIN32)
	OPTION(BRLCAD-ENABLE_X11 "Use X11." OFF)
elseif(BRLCAD-ENABLE_AQUA)
	OPTION(BRLCAD-ENABLE_X11 "Use X11." OFF)
else(WIN32)
	OPTION(BRLCAD-ENABLE_X11 "Use X11." ON)
endif(WIN32)
if (BRLCAD-ENABLE_X11)
	SET(BRLCAD-ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
	SET(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif (BRLCAD-ENABLE_X11)
IF(NOT APPLE)
	MARK_AS_ADVANCED(OPENGL_USE_AQUA)
ENDIF(NOT APPLE)

# Enable/disable features requiring the Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tk
OPTION(BRLCAD-ENABLE_TK "Enable features requiring the Tk toolkit" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_TK)
IF(NOT WIN32)
	IF (APPLE)
		IF(NOT BRLCAD-ENABLE_X11 AND NOT BRLCAD-ENABLE_AQUA)
			SET(BRLCAD-ENABLE_TK OFF)
		ENDIF(NOT BRLCAD-ENABLE_X11 AND NOT BRLCAD-ENABLE_AQUA)
		IF(BRLCAD-ENABLE_X11)
			SET(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
		ENDIF(BRLCAD-ENABLE_X11)
	ELSE (APPLE)
		IF(NOT BRLCAD-ENABLE_X11)
			SET(BRLCAD-ENABLE_TK OFF)
		ELSE(NOT BRLCAD-ENABLE_X11)
			SET(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
		ENDIF(NOT BRLCAD-ENABLE_X11)
	ENDIF(APPLE)
ENDIF(NOT WIN32)

# Enable features requring OPENGL
OPTION(BRLCAD-ENABLE_OPENGL "Use OpenGL." ON)

# Enable RTGL.  Requires an enabled OpenGL.
OPTION(BRLCAD-ENABLE_RTGL "Enable experimental RTGL code." OFF)
IF(NOT BRLCAD-ENABLE_OPENGL AND BRLCAD-ENABLE_RTGL)
	MESSAGE("RTGL requested, but OpenGL is not enabled - disabling")
	SET(BRLCAD-ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
ENDIF(NOT BRLCAD-ENABLE_OPENGL AND BRLCAD-ENABLE_RTGL)
IF(NOT BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_RTGL)
	MESSAGE("RTGL currently works only with GLX, and X11 is not enabled - disabling")
	SET(BRLCAD-ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
ENDIF(NOT BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_RTGL)
IF(BRLCAD-ENABLE_AQUA)
	SET(OPENGL_USE_AQUA ON CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
ENDIF(BRLCAD-ENABLE_AQUA)


# Enable JOVE.
OPTION(BRLCAD-ENABLE_JOVE "Enable jove editor build."	${BRLCAD-ENABLE_ALL_LOCAL_LIBS})


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
OPTION(BRLCAD-ENABLE_CXX_STRICT "Use strict compiler settings on cxx files in libraries that build with strict" OFF)
if (BRLCAD-ENABLE_STRICT)
MARK_AS_ADVANCED(BRLCAD-ENABLE_STRICT)
MARK_AS_ADVANCED(BRLCAD-ENABLE_CXX_STRICT)
	FILE(APPEND  ${CONFIG_H_FILE} "#define STRICT_FLAGS 1\n")
endif (BRLCAD-ENABLE_STRICT)

# Build with O3 compiler optimization.  This should be on for release builds
IF("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	OPTION(BRLCAD-ENABLE_OPTIMIZED_BUILD "Use optimized compiler settings" ON)
ELSE("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	OPTION(BRLCAD-ENABLE_OPTIMIZED_BUILD "Use optimized compiler settings" OFF)
ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Release")
MARK_AS_ADVANCED(BRLCAD-ENABLE_OPTIMIZED_BUILD)
IF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT BRLCAD-ENABLE_OPTIMIZED_BUILD)
	MESSAGE("Warning - build type is Release but optimization has been disabled")
ENDIF("${CMAKE_BUILD_TYPE}" MATCHES "Release" AND NOT BRLCAD-ENABLE_OPTIMIZED_BUILD)

# Build with compiler warning flags 
OPTION(BRLCAD-ENABLE_COMPILER_WARNINGS "Use compiler warning flags" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_COMPILER_WARNINGS)

# By default, we don't want any error reports at all from src/other
OPTION(BRLCAD-DISABLE_SRC_OTHER_WARN "Disable warnings for src/other" ON)
MARK_AS_ADVANCED(BRLCAD-DISABLE_SRC_OTHER_WARN)

# Build with full compiler lines visible by default (won't need make
# VERBOSE=1) on command line
OPTION(BRLCAD-ENABLE_VERBOSE_PROGRESS "verbose output" OFF)
IF(BRLCAD-ENABLE_VERBOSE_PROGRESS)
	SET(CMAKE_VERBOSE_MAKEFILE ON)
ENDIF(BRLCAD-ENABLE_VERBOSE_PROGRESS)

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
IF(MSVC)
	IF(CMAKE_CL_64)
		OPTION(BRLCAD-ENABLE_64BIT "MSVC 64 bit compiler" ON)
	ELSE(CMAKE_CL_64)
		OPTION(BRLCAD-ENABLE_64BIT "MSVC 32 bit compiler" ON)
	ENDIF(CMAKE_CL_64)
ELSE(MSVC)
	IF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
		OPTION(BRLCAD-ENABLE_64BIT "64 bit void pointer" ON)
	ELSE(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
		math(EXPR platform_bit_type "${CMAKE_SIZEOF_VOID_P} * 8")
		OPTION(BRLCAD-ENABLE_64BIT "${platform_bit_type} bit void pointer" OFF)
	ENDIF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
	FILE(APPEND  ${CONFIG_H_FILE} "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")
ENDIF(MSVC)
MARK_AS_ADVANCED(BRLCAD-ENABLE_64BIT)

# Take advantage of parallel processors if available - highly recommended
OPTION(BRLCAD-ENABLE_SMP "Enable SMP architecture parallel computation support" ON)
IF(WIN32)
	MESSAGE("SMP Support is not ready on Windows - disabling")
	SET(BRLCAD-ENABLE_SMP OFF)
ENDIF(WIN32)
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


# Load various wrapper macros for checking libraries, headers and 
# functions
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_CheckFunctions.cmake)

# The src/other subdirectory does not share (at least, not universally)
# certain definitions common to all other directories in BRL-CAD.  Also,
# we don't want flags that are otherwise global to BRL-CAD to be passed
# to the src/other directory (particularly things like strict compiler
# flags) so add src/other before any of that setup occurs.
ADD_SUBDIRECTORY(src/other)


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
MARK_AS_ADVANCED(CRYPT_LIBRARY)

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
	MARK_AS_ADVANCED(BRLCAD-ENABLE_RTSERVER)
ENDIF(NOT HAVE_JNI_H AND NOT HAVE_JAVAVM_JNI_H)

# Find X11 if it is enabled
IF(BRLCAD-ENABLE_X11)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindX11.cmake)
   BRLCAD_INCLUDE_FILE(X11/Xlib.h HAVE_X11_XLIB_H)
ENDIF(BRLCAD-ENABLE_X11)

#Only on WIN32 will the combination of the build search tests and
#libtermlib fail to find a valid term, so set variables accordingly
IF(NOT WIN32)
	SET(HAVE_TERMLIB 1 CACHE BOOL "Have some termlib" FORCE)
ELSE(NOT WIN32)
	SET(BRLCAD_BUILD_LOCAL_TERMLIB_FORCE_OFF ON CACHE BOOL "Disable libtermlib	build." FORCE)
	SET(HAVE_TERMLIB 0 CACHE BOOL "Have some termlib" FORCE)
ENDIF(NOT WIN32)
MARK_AS_ADVANCED(HAVE_TERMLIB)

# on SunOS with Sun Studio we need sunmath
CHECK_LIBRARY_EXISTS(sunmath ieee_flags "" HAVE_SUNMATH_LIBRARY)
IF(HAVE_SUNMATH_LIBRARY)
	SET(SUN_MATH_LIB "-lsunmath")
ENDIF(HAVE_SUNMATH_LIBRARY)

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
BRLCAD_INCLUDE_FILE(arpa/inet.h HAVE_ARPA_INET_H)
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
BRLCAD_INCLUDE_FILE(curses.h HAVE_CURSES_H)
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

SET(CMAKE_C_FLAGS "" CACHE STRING "compiler flags" FORCE)
SET(CMAKE_CXX_FLAGS "" CACHE STRING "cxx compiler flags" FORCE)
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
BRLCAD_FUNCTION_EXISTS(writev HAVE_WRITEV)


# *******************************************************************
# ***          Check for System Services - Stage 8 of 9           ***
# *******************************************************************

# We've done the toplevel configure steps, now add the subdirectories
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(db)
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(sh)
ADD_SUBDIRECTORY(misc)
ADD_SUBDIRECTORY(pix)
ADD_SUBDIRECTORY(bench)
ADD_SUBDIRECTORY(regress)

# Now that all the tests are done, configure the brlcad_config.h file:
FILE(APPEND ${CONFIG_H_FILE} "#endif /* __CONFIG_H__ */\n")
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)

# Before we finalize, set some specific global compiler flags
IF(MSVC)
	#SET(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -nologo -D_CRT_SECURE_NO_WARNINGS)
	SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -nologo -D_CRT_SECURE_NO_WARNINGS)
	SET(CMAKE_EXE_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
	SET(CMAKE_SHARED_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
	SET(CMAKE_MODULE_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT} /NOLOGO)
ENDIF(MSVC)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${CMAKE_PROJECT_NAME}_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${${CMAKE_PROJECT_NAME}_CXX_FLAGS}")

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

	SET(CMAKE_INSTALL_PREFIX_LABEL "Prefix")
	SET(${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR_LABEL "Binaries")
	SET(${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR_LABEL "Manual pages")
	SET(${CMAKE_PROJECT_NAME}_INSTALL_CONF_DIR_LABEL "Configration files")
	SET(${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR_LABEL "Data resource files")
	SET(PATHLABELS CMAKE_INSTALL_PREFIX ${CMAKE_PROJECT_NAME}_INSTALL_BIN_DIR ${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR ${CMAKE_PROJECT_NAME}_INSTALL_CONF_DIR ${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR)
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
		IF(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
			MESSAGE("${CURRENT_LABEL}: ${${path_label}}")
		ELSE(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
			MESSAGE("${CURRENT_LABEL}: ${CMAKE_INSTALL_PREFIX}/${${path_label}}")
		ENDIF(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
	ENDFOREACH() 
	MESSAGE("")

	STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
	IF(NOT BUILD_TYPE)
		SET(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS}")
		SET(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	ELSE(NOT BUILD_TYPE)
		SET(CMAKE_ALL_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BUILD_TYPE}}")
		SET(CMAKE_ALL_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE}}")
	ENDIF(NOT BUILD_TYPE)

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

	IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		SET(BRLCAD-ENABLE_DEBUG_BUILD ON)
	ELSE ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		SET(BRLCAD-ENABLE_DEBUG_BUILD OFF)
	ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")

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
	SET(BRLCAD-ENABLE_RUNTIME_DEBUG_LABEL "Enable run-time debugging (optional) ")
	if (BRLCAD-ENABLE_64BIT)
		set(BRLCAD_ARCH_BITSETTING "ON")
	else (BRLCAD-ENABLE_64BIT)
		math(EXPR platform_bit_type "${CMAKE_SIZEOF_VOID_P} * 8")
		set(BRLCAD_ARCH_BITSETTING "OFF (${platform_bit_type} bit)")
	endif (BRLCAD-ENABLE_64BIT)
	SET(BRLCAD_ARCH_BITSETTING_LABEL "Build 64-bit release ")
	SET(BRLCAD-ENABLE_OPTIMIZED_BUILD_LABEL "Build optimized release ")
	SET(BRLCAD-ENABLE_DEBUG_BUILD_LABEL "Build debug release ")
	SET(BRLCAD-ENABLE_PROFILING_LABEL "Build profile release ")
	SET(BRLCAD-ENABLE_SMP_LABEL "Build SMP-capable release ")
	SET(BUILD_STATIC_LIBS_LABEL "Build static libraries ")
	SET(BUILD_SHARED_LIBS_LABEL "Build dynamic libraries ")
	SET(BRLCAD-ENABLE_COMPILER_WARNINGS_LABEL "Print verbose compilation warnings ")
	SET(BRLCAD-ENABLE_VERBOSE_PROGRESS_LABEL "Print verbose compilation progress ")
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
	SET(SETTINGLIST2 BRLCAD-ENABLE_X11 BRLCAD-ENABLE_OPENGL BRLCAD-ENABLE_RTSERVER BRLCAD-ENABLE_RUNTIME_DEBUG )
	SET(SETTINGLIST3 BRLCAD_ARCH_BITSETTING BRLCAD-ENABLE_OPTIMIZED_BUILD BRLCAD-ENABLE_DEBUG_BUILD BRLCAD-ENABLE_PROFILING BRLCAD-ENABLE_SMP BUILD_STATIC_LIBS BUILD_SHARED_LIBS BRLCAD-ENABLE_COMPILER_WARNINGS BRLCAD-ENABLE_VERBOSE_PROGRESS BRLCAD-INSTALL_EXAMPLE_GEOMETRY BRLCAD-BUILD_DOCBUILD_MSG)
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
SET(BUILD_DELTA_FILE "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_FILE")
SET(BUILD_DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_START")
SET(BUILD_DELTA_END "${CMAKE_BINARY_DIR}/CMakeTmp/BUILD_DELTA_END")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/builddelta_start.c.in
	${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c)
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/builddelta_end.c.in
	${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c)
ADD_EXECUTABLE(printtimestamp ${BRLCAD_CMAKE_DIR}/test_srcs/print_timestamp.c)
ADD_EXECUTABLE(buildtimestart ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_start.c)
ADD_EXECUTABLE(buildtimeend ${CMAKE_BINARY_DIR}/CMakeTmp/builddelta_end.c)
ADD_CUSTOM_COMMAND(
	OUTPUT ${BUILD_DELTA_FILE}
	COMMAND buildtimestart
	COMMENT ""
	)
ADD_CUSTOM_TARGET(timestamp ALL	
	COMMAND printtimestamp
	COMMAND ${CMAKE_COMMAND} -E rename ${BUILD_DELTA_FILE} ${BUILD_DELTA_START}
	DEPENDS ${BUILD_DELTA_FILE}
	)
ADD_CUSTOM_TARGET(buildtimedelta ALL	
	COMMAND buildtimeend
	COMMAND ${CMAKE_COMMAND} -E remove ${BUILD_DELTA_START}
	)

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

# We want the timestamp to come first, so make ALL library,
# exec and external targets depend on timestamp (except for
# the exec target printtimestamp, which is needed by timestamp)
# Similarly, buildtimedelta needs to depend on every target.
get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
get_property(CMAKE_CUSTOM_TARGET_LIST GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST)
MARK_AS_ADVANCED(CMAKE_LIBRARY_TARGET_LIST)
MARK_AS_ADVANCED(CMAKE_EXEC_TARGET_LIST)
MARK_AS_ADVANCED(CMAKE_CUSTOM_TARGET_LIST)
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
		ADD_DEPENDENCIES(buildtimedelta ${libtarget})
	endif(target_location)
ENDFOREACH(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
FOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
	get_target_property(target_location ${exectarget} LOCATION)
	if(target_location)
		IF(NOT ${exectarget} MATCHES "printtimestamp"
				AND NOT ${custtarget} MATCHES "buildtimestart")
			ADD_DEPENDENCIES(${exectarget} timestamp)
		ENDIF()
		ADD_DEPENDENCIES(buildtimedelta ${exectarget})
	endif(target_location)
ENDFOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
FOREACH(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
		IF(NOT ${custtarget} MATCHES "timestamp")
			ADD_DEPENDENCIES(${custtarget} timestamp)
		ENDIF()
		IF(NOT ${custtarget} MATCHES "buildtimedelta")
			get_target_property(not_in_all ${custtarget} EXCLUDE_FROM_ALL)
			IF(NOT not_in_all)
				ADD_DEPENDENCIES(buildtimedelta ${custtarget})
			ENDIF(NOT not_in_all)
		ENDIF(NOT ${custtarget} MATCHES "buildtimedelta")
ENDFOREACH(custtarget ${CMAKE_CUSTOM_TARGET_LIST})
FOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
	get_target_property(target_confcmd ${externaltarget} _EP_CONFIGURE_COMMAND)
	if(target_confcmd)
		ADD_DEPENDENCIES(${externaltarget} timestamp)
		ADD_DEPENDENCIES(buildtimedelta ${externaltarget})
	endif(target_confcmd)
ENDFOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})

# We get one and only one noprod target - make it the
# top level, since that's the only sensible choice.
SET(CMAKE_TARGET_FILE_LIST "")
get_property(CMAKE_LIBRARY_TARGET_LIST GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST)
FOREACH(target ${CMAKE_LIBRARY_TARGET_LIST})
	get_target_property(targetfile ${target} LOCATION)
	get_target_property(targetversion ${target} VERSION)
	get_target_property(targetsoversion ${target} SOVERSION)
	SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}")
	IF(targetsoversion)
		SET(CMAKE_TARGET_FILE_LIST	"${CMAKE_TARGET_FILE_LIST};${targetfile}.${targetsoversion}")
	ENDIF(targetsoversion)
	IF(targetversion)
		SET(CMAKE_TARGET_FILE_LIST	"${CMAKE_TARGET_FILE_LIST};${targetfile}-${targetversion};${targetfile}.${targetversion}")
	ENDIF(targetversion)
ENDFOREACH(target ${CMAKE_LIBRARY_TARGET_LIST})
get_property(CMAKE_EXEC_TARGET_LIST GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST)
FOREACH(target ${CMAKE_EXEC_TARGET_LIST})
	get_target_property(targetfile ${target} LOCATION)
	get_target_property(targetversion ${target} VERSION)
	SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}")
	IF(targetversion)
		SET(CMAKE_TARGET_FILE_LIST "${CMAKE_TARGET_FILE_LIST};${targetfile}-${targetversion};${targetfile}.${targetversion}")
	ENDIF(targetversion)
ENDFOREACH(target ${CMAKE_EXEC_TARGET_LIST})

STRING(REGEX REPLACE ";" "\\\;" TARGET_FILES "${CMAKE_TARGET_FILE_LIST}")
SET(noprod_file "
SET(CMAKE_TARGET_FILE_LIST \"${TARGET_FILES}\")
FOREACH(file \${CMAKE_TARGET_FILE_LIST})
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
ENDFOREACH(file \${CMAKE_TARGET_FILE_LIST})
")
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/noprod ${noprod_file})
ADD_CUSTOM_TARGET(noprod "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/noprod")

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
FILE(WRITE ${BRLCAD_BINARY_DIR}/include/conf/PATH "\"${CMAKE_INSTALL_PREFIX}\"") 
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
MARK_AS_ADVANCED(countFile)

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
FILE(COPY ${toplevel_DOCFILES} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR})
INSTALL(FILES ${toplevel_DOCFILES} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR})


# CPack is used to produce tgz files, RPMS, etc.
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BRL-CAD - a powerful cross-platform open source solid modeling system")
SET(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
SET(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README)
SET(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
SET(CPACK_PACKAGE_VERSION_MAJOR ${BRLCAD_MAJOR_VERSION})
SET(CPACK_PACKAGE_VERSION_MINOR ${BRLCAD_MINOR_VERSION})
SET(CPACK_PACKAGE_VERSION_PATCH ${BRLCAD_PATCH_VERSION})

IF(UNIX)
	FOREACH(target ${CMAKE_EXEC_TARGET_LIST})
		get_target_property(targetfile ${target} LOCATION)
		IF(targetfile)
			SET(CPACK_STRIP_FILES "${CMAKE_STRIP_FILE_LIST};${targetfile}")
		ENDIF(targetfile)
	ENDFOREACH(target ${CMAKE_EXEC_TARGET_LIST})
ENDIF(UNIX)

IF(UNIX AND NOT APPLE)
	SET(CPACK_GENERATOR TGZ TBZ2)
	SET(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
ELSE(UNIX AND NOT APPLE)
	SET(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
ENDIF(UNIX AND NOT APPLE)

IF(WIN32)
   SET(CPACK_GENERATOR NSIS)
   SET(CPACK_NSIS_PACKAGE_NAME "BRL-CAD")
   SET(CPACK_SOURCE_DIR ${CMAKE_SOURCE_DIR})
   SET(CPACK_DATA_DIR "share\\\\brlcad\\\\${BRLCAD_VERSION}")
   # There is a bug in NSI that does not handle full unix paths properly. Make
   # sure there is at least one set of four (4) backlasshes.
   SET(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\brlcad.ico")
   SET(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\uninstall.ico")
   SET(CPACK_NSIS_DISPLAY_NAME "BRL-CAD")
   SET(CPACK_NSIS_MODIFY_PATH ON)
ENDIF(WIN32)

SET(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
"brlcad-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
SET(CPACK_SOURCE_IGNORE_FILES "\\\\.svn/")
INCLUDE(CPack)

SET(toplevel_ignore_files 
	${toplevel_DOCFILES}
	BUGS
	ChangeLog
	TODO
	TODO.cmake
	autogen.sh
	configure.ac
	configure.cmake.sh
	m4
	INSTALL.cmake
	README.cmake
	)
CMAKEFILES(${toplevel_ignore_files})

get_property(distcheck_list GLOBAL PROPERTY DISTCHECK_DIRS)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distchecklist.cmake ${distcheck_list})
configure_file(${BRLCAD_CMAKE_DIR}/distcheck_buildsys.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake @ONLY)
configure_file(${BRLCAD_CMAKE_DIR}/svncheck.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/svncheck.cmake @ONLY)
configure_file(${BRLCAD_CMAKE_DIR}/distcheck_success_message.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_success_message.cmake @ONLY)
ADD_CUSTOM_TARGET(distcheck
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_buildsys.cmake
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/svncheck.cmake
	COMMAND cpack --config ${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake
	COMMAND ${CMAKE_COMMAND} -E tar xvzf ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz
	COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
	COMMAND ${CMAKE_COMMAND} -E make_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
	COMMAND ${CMAKE_COMMAND} -E chdir _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build	${CMAKE_COMMAND} ../${CPACK_SOURCE_PACKAGE_FILE_NAME}	-DBRLCAD-ENABLE_ALL_LOCAL_LIBS=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
	COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build	-- -j3
	COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target regress
	COMMAND TIMEFRAME=1 ${CMAKE_COMMAND} --build	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target benchmark
	COMMAND ${CMAKE_COMMAND} --build _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build --target install
	COMMAND ${CMAKE_COMMAND} -E remove_directory	${CPACK_SOURCE_PACKAGE_FILE_NAME}
	COMMAND ${CMAKE_COMMAND} -E remove_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
	COMMAND TIMEFRAME=1 _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/bin/benchmark	run
	COMMAND _${CPACK_SOURCE_PACKAGE_FILE_NAME}-install/bin/benchmark	clean
	COMMAND ${CMAKE_COMMAND} -E remove summary
	COMMAND ${CMAKE_COMMAND} -E remove run-*.log
	COMMAND ${CMAKE_COMMAND} -E remove_directory	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/distcheck_success_message.cmake
	)
ADD_CUSTOM_TARGET(distclean
	COMMAND ${CMAKE_COMMAND} -E remove_directory	${CPACK_SOURCE_PACKAGE_FILE_NAME}
	COMMAND ${CMAKE_COMMAND} -E remove_directory _${CPACK_SOURCE_PACKAGE_FILE_NAME}-build
	COMMAND ${CMAKE_COMMAND} -E remove_directory	_${CPACK_SOURCE_PACKAGE_FILE_NAME}-install
	COMMAND ${CMAKE_COMMAND} -E remove summary
	COMMAND ${CMAKE_COMMAND} -E remove run-*.log
	COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_SOURCE_PACKAGE_FILE_NAME}.*
	)	


# Mark various miscellaneous things as advanced that we don't want in our
# default view
MARK_AS_ADVANCED(BRLCAD_PREFIX)
MARK_AS_ADVANCED(ENABLE_64_BIT)
MARK_AS_ADVANCED(IEEE_QUIET)
MARK_AS_ADVANCED(INET_QUIET)
MARK_AS_ADVANCED(CMAKE_BACKWARDS_COMPATIBILITY)

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
