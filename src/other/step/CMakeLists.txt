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
SET(CMAKE_MODULE_PATH "${SCL_CMAKE_DIR}" ${CMAKE_MODULE_PATH})

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
SET(CONFIG_TITLE "*** Configuring STEP Class Libraries Release ${SCL_VERSION}, Build ${CONFIG_DATE} ***")
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

# Need to work on this - CMake can detect flex and bison, but apparently not
# generic lex/yacc - need to update the Wireshark files to be a a graceful
# fallback to the FLEX/BISON logic, and (perhaps) wrap both in some way to
# hide any differences from the build code?
find_package(FLEX)
find_package(BISON)
if (NOT FLEX_FOUND)
    INCLUDE(${SCL_CMAKE_DIR}/FindLEX.cmake)
endif (NOT FLEX_FOUND)
if (NOT BISON_FOUND)
    INCLUDE(${SCL_CMAKE_DIR}/FindYACC.cmake)
endif (NOT BISON_FOUND)

CHECK_LIBRARY_EXISTS(stdc++ main "" HAVE_STDCCXX)

SCL_INCLUDE_FILE(ndir.h HAVE_NDIR_H)
SCL_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
SCL_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
SCL_INCLUDE_FILE(sysent.h HAVE_SYSENT_H)
SCL_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

SCL_FUNCTION_EXISTS(abs HAVE_ABS)
SCL_FUNCTION_EXISTS(memcpy HAVE_MEMCPY)
SCL_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)

SCL_TYPE_SIZE("ssize_t" SSIZE_T)

FILE(WRITE ${SCL_SOURCE_DIR}/include/conf/COUNT "1")
FILE(WRITE ${SCL_SOURCE_DIR}/include/conf/DATE "1")
FILE(WRITE ${SCL_SOURCE_DIR}/include/conf/HOST "1")
FILE(WRITE ${SCL_SOURCE_DIR}/include/conf/PATH "1")
FILE(WRITE ${SCL_SOURCE_DIR}/include/conf/USER "1")


# Now that all the tests are done, configure the scl_config.h file:
configure_file(${CONFIG_H_FILE} ${SCL_SOURCE_DIR}/include/scl_config.h)

include_directories(
        ${SCL_SOURCE_DIR}/include
)

ADD_SUBDIRECTORY(src/express)
ADD_SUBDIRECTORY(src/exppp)
ADD_SUBDIRECTORY(src/fedex_plus)
#ADD_SUBDIRECTORY(src/clstepcore)
#ADD_SUBDIRECTORY(src/cleditor)
#ADD_SUBDIRECTORY(src/cldai)
#ADD_SUBDIRECTORY(src/clutils)
