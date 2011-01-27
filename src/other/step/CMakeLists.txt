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
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)


# See HACKING for details on how to properly update the version
FILE(READ ${SCL_SOURCE_DIR}/include/conf/MAJOR MAJOR_VERSION)
STRING(STRIP ${MAJOR_VERSION} MAJOR_VERSION)
FILE(READ ${SCL_SOURCE_DIR}/include/conf/MINOR MINOR_VERSION)
STRING(STRIP ${MINOR_VERSION} MINOR_VERSION)
FILE(READ ${SCL_SOURCE_DIR}/include/conf/PATCH PATCH_VERSION)
STRING(STRIP ${PATCH_VERSION} PATCH_VERSION)

SET(SCL_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(SCL_CMAKE_DIR "${SCL_SOURCE_DIR}/misc/CMake")
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
  EXEC_PROGRAM(umask ARGS 022 OUTPUT_VARIABLE exec_out)
ENDIF(NOT WIN32)

# Set build type here. In this case, watch for a command line assignment
# and if there isn't one force set to basic in order to have a default
# set for the remainder of the configure.  May not be necessary, and if
# not should be removed - wait for finished build files to tell.
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Basic" "Debug" "Release")
if (NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Basic" CACHE STRING "Set build type" FORCE)
endif (NOT CMAKE_BUILD_TYPE)


#-----------------------------------------------------------------------------
# Output directories.
IF(NOT LIBRARY_OUTPUT_PATH)
  SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT LIBRARY_OUTPUT_PATH)
IF(NOT EXECUTABLE_OUTPUT_PATH)
  SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT EXECUTABLE_OUTPUT_PATH)

#-----------------------------------------------------------------------------
# Configure install locations. 

IF(NOT CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local/scl")
ENDIF(NOT CMAKE_INSTALL_PREFIX)

# We will need an scl_config.h.in file to hold all the #cmakedefine statements,
# which will in turn be used to generate an scl_conf.h file.  In autotools this
# process is handled by autoheader - in the case of CMake we wrap the CHECK_*
# functions and the creation of the entry in the brlcad_config.h.in file into
# one step via a macro.  To run this macro, the file must first be present, which
# we take care of here:

SET(CONFIG_H_FILE "${SCL_BINARY_DIR}/include/scl_config.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** Define statements for CMake ****/\n")

INCLUDE(${SCL_CMAKE_DIR}/SCL_ConfigFile.cmake)

OPTION(SCL-BUILD_EXPRESS_ONLY "Only build express parser." OFF)
MARK_AS_ADVANCED(SCL-BUILD_EXPRESS_ONLY)

INCLUDE (CheckLibraryExists)

INCLUDE(${SCL_CMAKE_DIR}/FindLEX.cmake)
INCLUDE(${SCL_CMAKE_DIR}/FindYACC.cmake)

SCL_INCLUDE_FILE(ndir.h HAVE_NDIR_H)
SCL_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
SCL_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
SCL_INCLUDE_FILE(sysent.h HAVE_SYSENT_H)
SCL_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

SCL_FUNCTION_EXISTS(abs HAVE_ABS)
SCL_FUNCTION_EXISTS(memcpy HAVE_MEMCPY)
SCL_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)

SCL_TYPE_SIZE("ssize_t" SSIZE_T)

# Now that all the tests are done, configure the scl_config.h file:
configure_file(${CONFIG_H_FILE} ${SCL_BINARY_DIR}/include/scl_config.h)

include_directories(
        ${SCL_SOURCE_DIR}/include
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
