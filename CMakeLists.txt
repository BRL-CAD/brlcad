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
# This CMakeLists.txt file based on the VTK CMakeLists.txt file 
# from VTK:
#
# Copyright (c) 1993-2008 Ken Martin, Will Schroeder, Bill Lorensen 
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright 
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in 
#   the documentation and/or other materials provided with the 
#   distribution.
#
# * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor
#   the names of any contributors may be used to endorse or promote 
#   products derived from this software without specific prior 
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
# AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***                 BRL-CAD's CMakeLists.txt                    ***
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

# See HACKING for details on how to properly update the version
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MAJOR MAJOR_VERSION)
STRING(STRIP ${MAJOR_VERSION} MAJOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MINOR MINOR_VERSION)
STRING(STRIP ${MINOR_VERSION} MINOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/PATCH PATCH_VERSION)
STRING(STRIP ${PATCH_VERSION} PATCH_VERSION)

SET(CAD_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
SET(BRLCAD_VERSION "${CAD_VERSION}")

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
SET(CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR}" ${CMAKE_MODULE_PATH})

# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables and set them
# to "C" so things like date output are as expected
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


# Unfortunately, CMake doesn't give you variables with current day, month,
# etc.  For Windows use d_rossberg's execute_process approach, which doesn't give individual
# day, month, etc. values but does give us something we can work with. Otherwise,
# use the date command.

# TODO - figure out REGEX to get desired variables out of win32 string 

IF(WIN32)
   execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE CONFIG_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
   execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE CONFIG_TS OUTPUT_STRIP_TRAILING_WHITESPACE)
# SET(TMPSTR "Wed 08/04/2010")
# SET(TMPSTR2 "06:57 PM")
 #  STRING(REGEX REPLACE ...
ELSE(WIN32)
   EXEC_PROGRAM(date ARGS +%d OUTPUT_VARIABLE CONFIG_DAY) 
   EXEC_PROGRAM(date ARGS +%m OUTPUT_VARIABLE CONFIG_MONTH) 
   EXEC_PROGRAM(date ARGS +%Y OUTPUT_VARIABLE CONFIG_YEAR)
   SET(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
   # try RFC 2822 for timestamp
   EXEC_PROGRAM(date ARGS -R 2>/dev/null OUTPUT_VARIABLE CONFIG_TS)
   IF (NOT CONFIG_TS) 
      EXEC_PROGRAM(date ARGS +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null OUTPUT_VARIABLE CONFIG_TS) 
   ENDIF(NOT CONFIG_TS)
   IF (NOT CONFIG_TS) 
      EXEC_PROGRAM(date ARGS OUTPUT_VARIABLE CONFIG_TS) 
   ENDIF(NOT CONFIG_TS)
ENDIF(WIN32)

# Set build type here?  May be useful for picking intelligent defaults
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
	"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
	"Set build type" FORCE)



# print out the title with a pretty box computed to wrap around
SET(CONFIG_TITLE "*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***")
STRING(LENGTH ${CONFIG_TITLE} CONFIG_TITLE_LENGTH)
SET(SEPARATOR_STRING "*")
STRING(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
WHILE(${CONFIG_TITLE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
   SET(SEPARATOR_STRING "${SEPARATOR_STRING}*")
   STRING(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
ENDWHILE(${CONFIG_TITLE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
MESSAGE(STATUS "${SEPARATOR_STRING}")
MESSAGE(STATUS "${CONFIG_TITLE}")
MESSAGE(STATUS "${SEPARATOR_STRING}")

# For NFS volumes, to ensure proper file creation.
IF(NOT WIN32)
  EXEC_PROGRAM(umask ARGS 022)
ENDIF(NOT WIN32)


# The location in which to install BRLCAD.  What is this for Windows?
# This would be a good setting to conditionalize based on a flag for debug
# vs. release vs. ?? - set different defaults accordingly.
IF(NOT BRLCAD_PREFIX)
  IF(NOT WIN32)
#    SET(BRLCAD_PREFIX "/usr/brlcad/brlcad-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
#    SET(BRLCAD_PREFIX "${BRLCAD_SOURCE_DIR}/../brlcad-install")
    SET(BRLCAD_PREFIX "/usr/brlcad")
  ENDIF(NOT WIN32)
ENDIF(NOT BRLCAD_PREFIX)

SET(CMAKE_INSTALL_PREFIX "${BRLCAD_PREFIX}" CACHE STRING "prefix" FORCE)

# TODO - need to replace the functionality of BC_BRLCAD_ROOT and BC_BRLCAD_DATA here,
# if it's not already being handled - the data directory at least is likely to need some help

#-----------------------------------------------------------------------------
# Output directories.
IF(NOT LIBRARY_OUTPUT_PATH)
  SET(LIBRARY_OUTPUT_PATH ${BRLCAD_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT LIBRARY_OUTPUT_PATH)
IF(NOT EXECUTABLE_OUTPUT_PATH)
  SET(EXECUTABLE_OUTPUT_PATH ${BRLCAD_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT EXECUTABLE_OUTPUT_PATH)
SET(BRLCAD_LIBRARY_DIR ${LIBRARY_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})
SET(BRLCAD_EXECUTABLE_DIR ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR})

#-----------------------------------------------------------------------------
# Configure install locations.  This allows parent projects to modify
# the install location.  Specify a BRLCAD_PREFIX which prefixes
# all install targets.
# The location in which to install BRLCAD executables.
IF(NOT BRLCAD_INSTALL_BIN_DIR)
  SET(BRLCAD_INSTALL_BIN_DIR ${BRLCAD_PREFIX}/bin)
ENDIF(NOT BRLCAD_INSTALL_BIN_DIR)

# The location in which to install BRLCAD header files.
IF(NOT BRLCAD_INSTALL_INCLUDE_DIR)
  SET(BRLCAD_INSTALL_INCLUDE_DIR
    ${BRLCAD_PREFIX}/include)
ENDIF(NOT BRLCAD_INSTALL_INCLUDE_DIR)

# The location in which to install BRL-CAD libraries.
IF(NOT BRLCAD_INSTALL_LIB_DIR)
  SET(BRLCAD_INSTALL_LIB_DIR 
    ${BRLCAD_PREFIX}/lib)
ENDIF(NOT BRLCAD_INSTALL_LIB_DIR)

# The location in which to install CMake scripts for packaging BRLCAD.
IF(NOT BRLCAD_INSTALL_PACKAGE_DIR)
  SET(BRLCAD_INSTALL_PACKAGE_DIR ${BRLCAD_INSTALL_LIB_DIR})
ENDIF(NOT BRLCAD_INSTALL_PACKAGE_DIR)

# The location in which to install BRL-CAD documentation files 
IF(NOT BRLCAD_INSTALL_DOC_DIR)
  SET(BRLCAD_INSTALL_DOC_DIR ${BRLCAD_PREFIX}/share/brlcad/doc)
ENDIF(NOT BRLCAD_INSTALL_DOC_DIR)

# The location in which to install BRL-CAD doxygen documentation helper
# files.
IF(NOT BRLCAD_INSTALL_DOXYGEN_DIR)
  SET(BRLCAD_INSTALL_DOXYGEN_DIR ${BRLCAD_INSTALL_DOC_DIR}/doxygen)
ENDIF(NOT BRLCAD_INSTALL_DOXYGEN_DIR)

# We will need a brlcad_config.h.in file to hold all the #cmakedefine statements,
# which will in turn be used to generate a brlcad_conf.h file.  In autotools this
# process is handled by autoheader - in the case of CMake we wrap the CHECK_*
# functions and the creation of the entry in the brlcad_config.h.in file into
# one step via a macro.  To run this macro, the file must first be present, which
# we take care of here:

SET(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** Define statements for CMake ****/\n")

#--------------------------------------------------------------------------------
# Now we define the various options for BRL-CAD things the user can enable and
# disable.  As much as possible, autodetect sane default options.

# Sometimes an option no longer makes sense in light of other options,
# or a new option becomes available when others are enabled - load a
# macro to help with those cases.
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_Dependent_Options.cmake)


OPTION(BRLCAD-BUILD_BENCHMARK_ONLY "Only build benchmark." OFF)
MARK_AS_ADVANCED(BRLCAD-BUILD_BENCHMARK_ONLY)
OPTION(BRLCAD-BUILD_RTSERVER_ONLY "Only build rtserver." OFF)
MARK_AS_ADVANCED(BRLCAD-BUILD_RTSERVER_ONLY)


INCLUDE(${BRLCAD_CMAKE_DIR}/FindLEX.cmake)
INCLUDE(${BRLCAD_CMAKE_DIR}/FindYACC.cmake)


# This is the top level control property for handling library building 
set(BRLCAD-BUILD_SRC_OTHER "Auto" CACHE STRING
	"Set build type" FORCE)
set_property(CACHE BRLCAD-BUILD_SRC_OTHER PROPERTY STRINGS
	"Yes" "No" "Auto")

INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_ConfigFile.cmake)
BRLCAD_FUNCTION_EXISTS(drand48 HAVE_DRAND48)
BRLCAD_FUNCTION_EXISTS(strchr HAVE_STRCHR)
BRLCAD_FUNCTION_EXISTS(kill HAVE_KILL)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(grp.h HAVE_GRP_H)
BRLCAD_INCLUDE_FILE(pthread.h HAVE_PTHREAD)
BRLCAD_INCLUDE_FILE(pwd.h HAVE_PWD_H)
BRLCAD_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
BRLCAD_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
BRLCAD_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
BRLCAD_TYPE_SIZE("ssize_t" SSIZE_T)

FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/COUNT "1")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/DATE "1")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/HOST "1")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/PATH "1")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/USER "1")


# Now that all the tests are done, configure the brlcad_config.h file:
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_ThirdParty.cmake)


#---------------------------------------------------------------------------------
# Optionally Building Local Copies of External Libraries 
#
# Need to get a bit fancy here - since we want to be able to turn local libs
# on/off at will, need to be careful about how we handle key variables. Approach
# is to stash values in BRLCAD_* prefixed versions, then restore them after
# the various Find*.cmake commands do their thing.  If NOT doing local build,
# then the BRLCAD_* and standard variables will be the same.  It remains to be
# seen if there will be enough consistency for a macro - the variables that
# need to be overridden will vary depending on the particular tests made by each
# subdirectory's CMake logic.
#---------------------------------------------------------------------------------

# This is our "heavy hammer" flag - if this is set, assume system for
# everything.  As yet, there is no "automatic" logic to toggle specific
# libraries on if detection fails, but that may be possible.
OPTION(BRLCAD_USE_SYSTEM_LIBS "Use the system's libraries." OFF)

# zlib Library
BRLCAD_THIRD_PARTY_OPTION(ZLIB zlib)
BRLCAD_THIRD_PARTY_SUBDIR(ZLIB src/other/libz)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_ZLIB ON CACHE BOOL "Use the system's zlib library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_ZLIB)
   SET(BRLCAD_ZLIB_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libz" CACHE STRING "Directory containing zlib headers" FORCE)
   SET(ZLIB_INCLUDE_DIR "${BRLCAD_ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
   SET(BRLCAD_ZLIB_LIBRARY "zlib" CACHE STRING "zlib" FORCE)
ELSE(NOT BRLCAD_USE_SYSTEM_ZLIB)
   SET(BRLCAD_ZLIB_INCLUDE_DIR "${ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
   SET(BRLCAD_ZLIB_LIBRARY "${ZLIB_LIBRARY}" CACHE STRING "zlib library" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_ZLIB)

# libpng Library - Checks for ZLIB, so need to restore BRLCAD_ZLIB* settings - a
# successful search by FindZLIB will reset the basic variables.  If not using local
# copy of zlib, FindZLIB results will be identical in both cases so there is no
# danger of harming the libpng setup by changing BRLCAD_ZLIB*.
BRLCAD_THIRD_PARTY_OPTION(PNG libpng)
SET(ZLIB_INCLUDE_DIR "${BRLCAD_ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
SET(ZLIB_LIBRARY "${BRLCAD_ZLIB_LIBRARY}" CACHE STRING "zlib library" FORCE)
BRLCAD_THIRD_PARTY_SUBDIR(PNG src/other/libpng)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_PNG ON CACHE BOOL "Use the system's libpng library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_PNG)
   SET(BRLCAD_PNG_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libpng" CACHE STRING "Directory containing libpng headers" FORCE)
   # BRL-CAD needs STDIO - turn it on
   SET(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
   # PNG_LIB_NAME doesn't automatically propogate to toplevel due to scoping of CMake variables - 
   # use get_directory_property to recover the value and place it in the CACHE
   get_directory_property(BRLCAD_PNG_LIB_NAME DIRECTORY src/other/libpng DEFINITION PNG_LIB_NAME)
   SET(BRLCAD_PNG_LIB_NAME "${BRLCAD_PNG_LIB_NAME}" CACHE STRING "libpng name for targets" FORCE)
ELSE(NOT BRLCAD_USE_SYSTEM_PNG)
   SET(BRLCAD_PNG_INCLUDE_DIR "${PNG_PNG_INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers" FORCE)
   SET(BRLCAD_PNG_LIB_NAME "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_PNG)

# libregex Library
BRLCAD_THIRD_PARTY_OPTION(REGEX libregex)
BRLCAD_THIRD_PARTY_SUBDIR(REGEX src/other/libregex)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_REGEX ON CACHE BOOL "Use the system's libregex library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_REGEX)
   SET(BRLCAD_REGEX_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libregex" CACHE STRING "Directory containing libregex headers" FORCE)
   SET(REGEX_INCLUDE_DIR "${BRLCAD_REGEX_INCLUDE_DIR}" CACHE STRING "Directory containing libregex headers" FORCE)
   SET(BRLCAD_REGEX_LIBRARY "libregex" CACHE STRING "libregex" FORCE)
ELSE(NOT BRLCAD_USE_SYSTEM_REGEX)
   SET(BRLCAD_REGEX_INCLUDE_DIR "${REGEX_INCLUDE_DIR}" CACHE STRING "Directory containing libregex headers" FORCE)
   SET(BRLCAD_REGEX_LIBRARY "${REGEX_LIBRARY}" CACHE STRING "libregex library" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_REGEX)

# libutahrle Library
BRLCAD_THIRD_PARTY_OPTION(UTAHRLE libutahrle)
BRLCAD_THIRD_PARTY_SUBDIR(UTAHRLE src/other/libutahrle)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_UTAHRLE ON CACHE BOOL "Use the system's libutahrle library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_UTAHRLE)
   SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle" CACHE STRING "Directory containing libutahrle headers" FORCE)
   SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_UTAHRLE_INCLUDE_DIR}" CACHE STRING "Directory containing libutahrle headers" FORCE)
   SET(BRLCAD_UTAHRLE_LIBRARY "libutahrle" CACHE STRING "libutahrle" FORCE)
ELSE(NOT BRLCAD_USE_SYSTEM_UTAHRLE)
   SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "Directory containing libutahrle headers" FORCE)
   SET(BRLCAD_UTAHRLE_LIBRARY "${UTAHRLE_LIBRARY}" CACHE STRING "libutahrle library" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_UTAHRLE)

# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.

INCLUDE(${BRLCAD_CMAKE_DIR}/FindTCL.cmake)

# Tk
#include(ExternalProject)
#ExternalProject_Add(
#	tk
#	DOWNLOAD_COMMAND ""
#	PREFIX "${BRLCAD_PREFIX}"
#	SOURCE_DIR "${BRLCAD_SOURCE_DIR}/src/other/tk"
#	CONFIGURE_COMMAND
#	autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/tk/unix -o ${BRLCAD_SOURCE_DIR}/src/other/tk/unix/configure ${BRLCAD_SOURCE_DIR}/src/other/tk/unix/configure.in && ${BRLCAD_SOURCE_DIR}/src/other/tk/unix/configure --with-tk=${BRLCAD_SOURCE_DIR}/src/other/tk/unix 
#        BUILD_COMMAND make
#)

ADD_SUBDIRECTORY(src/libbu)
ADD_SUBDIRECTORY(src/libbn)
