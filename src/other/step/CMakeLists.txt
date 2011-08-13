#                     C M A K E L I S T S . T X T
# SCL
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
PROJECT(SCL)

# build shared libs by default
set(BUILD_SHARED_LIBS ON)

# build static libs by default
set(BUILD_STATIC_LIBS ON)

# Set version
SET(SCL_VERSION_MAJOR "3")
SET(SCL_VERSION_MINOR "2")
SET(SCL_VERSION_PATCH "0")
SET(SCL_VERSION "${SCL_VERSION_MAJOR}.${SCL_VERSION_MINOR}.${SCL_VERSION_PATCH}")

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(SCL_CMAKE_DIR "${SCL_SOURCE_DIR}/CMake")
SET(CMAKE_MODULE_PATH "${SCL_CMAKE_DIR};${CMAKE_MODULE_PATH}")

INCLUDE(${SCL_CMAKE_DIR}/SCL_Utils.cmake)

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

# For NFS volumes, to ensure proper file creation.
IF(NOT WIN32)
	IF(NOT UMASK)
		EXEC_PROGRAM(umask ARGS 022 OUTPUT_VARIABLE exec_out)
	ELSE(NOT UMASK)
		EXEC_PROGRAM(umask ARGS ${UMASK} OUTPUT_VARIABLE exec_out)
	ENDIF(NOT UMASK)
ENDIF(NOT WIN32)

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


#-----------------------------------------------------------------------------
# Output directories.
IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
	IF(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
	ELSE(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
	ENDIF(WIN32)
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
	IF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
		IF(WIN32)
			SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
		ELSE(WIN32)
			SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
		ENDIF(WIN32)
	ENDIF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
	IF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
		SET("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
	ENDIF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
	IF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
		SET("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
	ENDIF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
ENDFOREACH()


#-----------------------------------------------------------------------------
# Configure install locations. 

# The location in which to install BRLCAD. Need a good Debug location 
# for Windows.  Only do this if CMAKE_INSTALL_PREFIX hasn't been set
# already, to try and allow parent builds (if any) some control.
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	MESSAGE("HAVE_DEFAULT_PREFIX")
	IF(NOT WIN32)
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(CMAKE_INSTALL_PREFIX "/usr")
		ELSEIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			MESSAGE("setting debug output dir")
			SET(CMAKE_INSTALL_PREFIX "${SCL_SOURCE_DIR}/../scl-install")
		ELSE("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(CMAKE_INSTALL_PREFIX "/usr/local")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	ENDIF(NOT WIN32)
	SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "SCL install prefix" FORCE)
	SET(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
SET(BRLCAD_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "SCL install prefix")

OPTION(SCL-BUILD_EXPRESS_ONLY "Only build express parser." OFF)
MARK_AS_ADVANCED(SCL-BUILD_EXPRESS_ONLY)

# Take the scl config file template and copy it to the build directory so CMake
# scripts can append to it if need be
configure_file(${SCL_SOURCE_DIR}/include/scl_cf_cmake.h.in ${SCL_BINARY_DIR}/include/scl_cf.h.in COPYONLY)
SET(CONFIG_H_FILE ${SCL_BINARY_DIR}/include/scl_cf.h.in)

INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckTypeSize)
INCLUDE(${SCL_CMAKE_DIR}/FindLEX.cmake)
INCLUDE(${SCL_CMAKE_DIR}/FindYACC.cmake)

CHECK_INCLUDE_FILE(ndir.h HAVE_NDIR_H)
CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sysent.h HAVE_SYSENT_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

CHECK_FUNCTION_EXISTS(abs HAVE_ABS)
CHECK_FUNCTION_EXISTS(memcpy HAVE_MEMCPY)
CHECK_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)

CHECK_TYPE_SIZE("ssize_t" SSIZE_T)

# Now that all the tests are done, configure the scl_cf.h file:
configure_file(${SCL_BINARY_DIR}/include/scl_cf.h.in ${SCL_BINARY_DIR}/include/scl_cf.h)

add_definitions(
	-DHAVE_CONFIG_H
	)

include_directories(
	${SCL_SOURCE_DIR}/include
	${SCL_BINARY_DIR}/include
	)

ADD_SUBDIRECTORY(src/express)
ADD_SUBDIRECTORY(src/exppp)
ADD_SUBDIRECTORY(src/fedex_plus)
ADD_SUBDIRECTORY(src/clstepcore)
ADD_SUBDIRECTORY(src/cleditor)
ADD_SUBDIRECTORY(src/cldai)
ADD_SUBDIRECTORY(src/clutils)
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(data)
ADD_SUBDIRECTORY(doc)
