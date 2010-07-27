#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2004-2010 United States Government as represented by
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

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
SET(CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR}" ${CMAKE_MODULE_PATH})

#--------------------------------------------------------------------
# BRL-CAD version numbers are read from files - to do this, support 
# for reading the values into variables via cat is needed (thanks
# to Andy Cedilnik on the CMake list for demonstrating how to use
# this technique with CMake)
# http://www.cmake.org/pipermail/cmake/2003-March/003486.html

SET(cat_prog cat)
IF(WIN32)
  IF(NOT UNIX)
    SET(cat_prog type)
  ENDIF(NOT UNIX)
ENDIF(WIN32)

EXEC_PROGRAM(${cat_prog} ARGS ${BRLCAD_SOURCE_DIR}/include/conf/MAJOR OUTPUT_VARIABLE BRLCAD_MAJOR_VERSION)
EXEC_PROGRAM(${cat_prog} ARGS ${BRLCAD_SOURCE_DIR}/include/conf/MINOR OUTPUT_VARIABLE BRLCAD_MINOR_VERSION)
EXEC_PROGRAM(${cat_prog} ARGS ${BRLCAD_SOURCE_DIR}/include/conf/PATCH OUTPUT_VARIABLE BRLCAD_PATCH_VERSION)

SET(BRLCAD_VERSION
    "${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")


# Unfortunately, CMake doesn't give you variables with current day, month,
# etc.  For Windows use d_rossberg's execute_process, which doesn't give individual
# day, month, etc. values but does give us something for the banner. Otherwise,
# use the date command.
IF(WIN32)
   execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE CONFIG_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
ELSE(WIN32)
   EXEC_PROGRAM(date ARGS +%d OUTPUT_VARIABLE CONFIG_DAY) 
   EXEC_PROGRAM(date ARGS +%m OUTPUT_VARIABLE CONFIG_MONTH) 
   EXEC_PROGRAM(date ARGS +%Y OUTPUT_VARIABLE CONFIG_YEAR)
   SET(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
ENDIF(WIN32)

# CMake doesn't seem to allow for the fancy separator logic to generate "*"
# strings of the right length, so use extra lines around the title
MESSAGE(STATUS)
MESSAGE(STATUS)
MESSAGE(STATUS "*** Configuring BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ***")
MESSAGE(STATUS)
MESSAGE(STATUS)

# For NFS volumes, to ensure proper file creation
IF(NOT WIN32)
  EXEC_PROGRAM(umask ARGS 022)
ENDIF(NOT WIN32)

#-----------------------------------------------------------------------------
# Load some macros.
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_ThirdParty.cmake)

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
# the install location.  Specify a BRLCAD_INSTALL_ROOT which prefixes
# all install targets.

# The location in which to install BRLCAD.
IF(NOT BRLCAD_INSTALL_ROOT)
  IF(NOT WIN32)
    SET(BRLCAD_INSTALL_ROOT /usr/brlcad/brlcad-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION})
  ENDIF(NOT WIN32)
ENDIF(NOT BRLCAD_INSTALL_ROOT)

SET(CMAKE_INSTALL_PREFIX ${BRLCAD_INSTALL_ROOT})
SET(CMAKE_INSTALL_PREFIX "${BRLCAD_INSTALL_ROOT}" CACHE STRING "prefix" FORCE)

# The location in which to install BRLCAD executables.
IF(NOT BRLCAD_INSTALL_BIN_DIR)
  SET(BRLCAD_INSTALL_BIN_DIR ${BRLCAD_INSTALL_ROOT}/bin)
ENDIF(NOT BRLCAD_INSTALL_BIN_DIR)

# The location in which to install BRLCAD header files.
IF(NOT BRLCAD_INSTALL_INCLUDE_DIR)
  SET(BRLCAD_INSTALL_INCLUDE_DIR
    ${BRLCAD_INSTALL_ROOT}/include)
ENDIF(NOT BRLCAD_INSTALL_INCLUDE_DIR)

# The location in which to install BRL-CAD libraries.
IF(NOT BRLCAD_INSTALL_LIB_DIR)
  SET(BRLCAD_INSTALL_LIB_DIR 
    ${BRLCAD_INSTALL_ROOT}/lib)
ENDIF(NOT BRLCAD_INSTALL_LIB_DIR)

# The location in which to install CMake scripts for packaging BRLCAD.
IF(NOT BRLCAD_INSTALL_PACKAGE_DIR)
  SET(BRLCAD_INSTALL_PACKAGE_DIR ${BRLCAD_INSTALL_LIB_DIR})
ENDIF(NOT BRLCAD_INSTALL_PACKAGE_DIR)

# The location in which to install BRL-CAD documentation files that
# are not automatically generated.
IF(NOT BRLCAD_INSTALL_DOC_DIR)
  SET(BRLCAD_INSTALL_DOC_DIR ${BRLCAD_INSTALL_PACKAGE_DIR}/doc)
ENDIF(NOT BRLCAD_INSTALL_DOC_DIR)

# The location in which to install BRL-CAD doxygen documentation helper
# files.
IF(NOT BRLCAD_INSTALL_DOXYGEN_DIR)
  SET(BRLCAD_INSTALL_DOXYGEN_DIR ${BRLCAD_INSTALL_PACKAGE_DIR}/doc/doxygen)
ENDIF(NOT BRLCAD_INSTALL_DOXYGEN_DIR)

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
ELSE(NOT BRLCAD_USE_SYSTEM_PNG)
   SET(BRLCAD_PNG_INCLUDE_DIR "${PNG_INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers" FORCE)
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


