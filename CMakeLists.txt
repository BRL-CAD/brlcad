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

# Repeated runs of cmake wil result in this variable being set during
# the Find* routines, which will return libraries from previous
# installs.  Re-set this at the beginning of the file - this ensures
# that Find* results are consistent between the first run and
# previous runs
SET(CMAKE_INSTALL_PREFIX "")

# This file contains the top level CMakeLists.txt logic for the 
# BRL-CAD software package.
# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(BRLCAD)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# Will need our own add_executable and add_library
# versions in order to keep track of targets and
# define rules for all products.
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


# See HACKING for details on how to properly update the version
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MAJOR BRLCAD_MAJOR_VERSION)
STRING(STRIP ${BRLCAD_MAJOR_VERSION} BRLCAD_MAJOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/MINOR BRLCAD_MINOR_VERSION)
STRING(STRIP ${BRLCAD_MINOR_VERSION} BRLCAD_MINOR_VERSION)
FILE(READ ${BRLCAD_SOURCE_DIR}/include/conf/PATCH BRLCAD_PATCH_VERSION)
STRING(STRIP ${BRLCAD_PATCH_VERSION} BRLCAD_PATCH_VERSION)

SET(CAD_VERSION "${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
SET(BRLCAD_VERSION "${CAD_VERSION}")

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(BRLCAD_CMAKE_DIR "${BRLCAD_SOURCE_DIR}/misc/CMake")
SET(CMAKE_MODULE_PATH "${BRLCAD_CMAKE_DIR};${CMAKE_MODULE_PATH}")

# Load utility macros that will be used throughout the BRL-CAD
# build logic
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_Util.cmake)

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

# Write out entries to populate a tm struct to be used for time deltas later
SET(DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_START")
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/timedelta_start.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c
	OUTPUT_VARIABLE COMPILEMESSAGES)

# Unfortunately, CMake doesn't give you variables with current day, month,
# etc.  There are several possible approaches to this, but most (e.g. the date command)
# are not cross platform. We build a small C file which writes out the needed values
# to files in the build directory. Those files are then read and stripped by CMake.
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

# The location in which to install BRLCAD.  What is this for Windows?
IF(NOT BRLCAD_PREFIX)
	IF(NOT WIN32)
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
			SET(BRLCAD_PREFIX "/usr/brlcad/rel-${BRLCAD_MAJOR_VERSION}.${BRLCAD_MINOR_VERSION}.${BRLCAD_PATCH_VERSION}")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Release")
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			SET(BRLCAD_PREFIX "${BRLCAD_SOURCE_DIR}/../brlcad-install")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
		IF ("${CMAKE_BUILD_TYPE}" MATCHES "Basic")
			SET(BRLCAD_PREFIX "/usr/brlcad")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Basic")
		if (NOT BRLCAD_PREFIX)
			SET(BRLCAD_PREFIX "/usr/brlcad")
		endif (NOT BRLCAD_PREFIX)
	ELSE(NOT WIN32)
		SET(BRLCAD_PREFIX "Program Files/BRL-CAD/")
	ENDIF(NOT WIN32)
ENDIF(NOT BRLCAD_PREFIX)

SET(CMAKE_INSTALL_PREFIX "${BRLCAD_PREFIX}" CACHE STRING "prefix" FORCE)

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

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# TODO - need to replace the functionality of BC_BRLCAD_ROOT and BC_BRLCAD_DATA here,
# if it's not already being handled - the data directory at least is likely to need some help
# also will need to set CFLAGS for debug, optimized, what have you - likely to need some
# options for all of that, although can probably use the build type setting to make intelligent
# default choices

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

# The location in which to install BRL-CAD configuration files.
IF(NOT BRLCAD_INSTALL_CONF_DIR)
	SET(BRLCAD_INSTALL_CONF_DIR ${BRLCAD_PREFIX}/etc)
ENDIF(NOT BRLCAD_INSTALL_CONF_DIR)

# The location in which to install CMake scripts for packaging BRLCAD.
IF(NOT BRLCAD_INSTALL_PACKAGE_DIR)
	SET(BRLCAD_INSTALL_PACKAGE_DIR ${BRLCAD_INSTALL_LIB_DIR})
ENDIF(NOT BRLCAD_INSTALL_PACKAGE_DIR)

# The location in which to install BRL-CAD data files 
IF(NOT BRLCAD_INSTALL_DATA_DIR)
	IF (${BRLCAD_PREFIX} MATCHES "^/usr[/]*$")
		SET(BRLCAD_INSTALL_DATA_DIR ${BRLCAD_PREFIX}/share/brlcad)
	ENDIF()
	IF (${BRLCAD_PREFIX} MATCHES "^/usr/local[/]*$")
		SET(BRLCAD_INSTALL_DATA_DIR ${BRLCAD_PREFIX}/share/brlcad)
	ENDIF()
	IF(NOT BRLCAD_INSTALL_DATA_DIR)
		SET(BRLCAD_INSTALL_DATA_DIR ${BRLCAD_PREFIX}/share)
	ENDIF()
ENDIF(NOT BRLCAD_INSTALL_DATA_DIR)

# The location in which to install BRL-CAD Manual pages
IF(NOT BRLCAD_INSTALL_MAN_DIR)
	SET(BRLCAD_INSTALL_MAN_DIR ${BRLCAD_PREFIX}/share/man)
	SET(MAN_DIR "share/man")
ENDIF(NOT BRLCAD_INSTALL_MAN_DIR)


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

# CMake can detect flex and bison, but apparently not generic lex/yacc.
# Solution is to make our own generic lex/yacc Find routines that look
# first for flex/bison, but will take whatever's available.  Since the
# BRL-CAD project isn't supposed to rely on flex/bison specific features
# anyway, there shouldn't be a problem.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindLEX.cmake)
INCLUDE(${BRLCAD_CMAKE_DIR}/FindYACC.cmake)

#--------------------------------------------------------------------------------
# Now we define the various options for BRL-CAD things the user can enable and
# disable.  As much as possible, autodetect sane default options.

OPTION(BRLCAD-ENABLE_STRICT "Use strict compiler settings" OFF)
if (BRLCAD-ENABLE_STRICT)
	FILE(APPEND  ${CONFIG_H_FILE} "#define STRICT_FLAGS 1\n")
	SET(CMAKE_C_FLAGS "-pedantic -W -Wall -Werror -Wno-long-long")
	SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
endif (BRLCAD-ENABLE_STRICT)

OPTION(BRLCAD-ENABLE_SMP "Enable SMP architecture parallel computation support" ON)
MARK_AS_ADVANCED(BRLCAD-ENABLE_SMP)
if (BRLCAD-ENABLE_SMP)
	FILE(APPEND  ${CONFIG_H_FILE} "#define PARALLEL 1\n")
endif (BRLCAD-ENABLE_SMP)



# Sometimes an option no longer makes sense in light of other options,
# or a new option becomes available when others are enabled - load a
# macro to help with those cases.
INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_Dependent_Options.cmake)

INCLUDE(${BRLCAD_CMAKE_DIR}/BRLCAD_ConfigFile.cmake)


OPTION(BRLCAD-BUILD_BENCHMARK_ONLY "Only build benchmark." OFF)
MARK_AS_ADVANCED(BRLCAD-BUILD_BENCHMARK_ONLY)
OPTION(BRLCAD-BUILD_RTSERVER_ONLY "Only build rtserver." OFF)
MARK_AS_ADVANCED(BRLCAD-BUILD_RTSERVER_ONLY)


# Use CMake techniques for finding libraries that BRL-CAD
# doesn't supply locally, but pay attention to which options
# are enabled

find_package(Threads)
find_package(Curses)
if (CURSES_HAVE_CURSES_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_CURSES_H 1\n")
endif()

if(WIN32)
	OPTION(BRLCAD-ENABLE_X11 "Use X11." OFF)
else()
	OPTION(BRLCAD-ENABLE_X11 "Use X11." ON)
endif(WIN32)
if (BRLCAD-ENABLE_X11)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindX11.cmake)
endif (BRLCAD-ENABLE_X11)

OPTION(BRLCAD-ENABLE_OPENGL "Use OpenGL." OFF)
if (BRLCAD-ENABLE_OPENGL)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
endif (BRLCAD-ENABLE_OPENGL)

INCLUDE (CheckLibraryExists)

# This may be overkill - does CHECK_LIBRARY_EXISTS usably set
# anything for us?  May want to wrap some combination of
# CHECK_LIBRARY_EXISTS and FIND_LIBRARY into a macro...
SET(BSD_LIST "bsd;c")
FOREACH(bsdcandidate ${BSD_LIST})
   IF(NOT BSD_LIBRARY)
     CHECK_LIBRARY_EXISTS(${bsdcandidate} daemon "" HAVE_BSDDAEMON)
     if (HAVE_BSDDAEMON)
	find_library(BSD_LIBRARY NAMES ${bsdcandidate})
     endif (HAVE_BSDDAEMON)
   ENDIF(NOT BSD_LIBRARY)
ENDFOREACH(bsdcandidate ${BSD_LIST})

# rework this - check maybe should come after library
# is successfully found? Also, need to make sure 
# paths used by CHECK_LIBRARY_EXISTS are reflected in
# find_library's PATHS list - see above macro comments...
if(NOT WIN32)
	CHECK_LIBRARY_EXISTS(m cos "" HAVE_MLIBRARY)
	if(HAVE_MLIBRARY)
		find_library(M_LIBRARY
			NAMES m
			PATHS /usr/lib /usr/local/lib
			)
	endif()
else()
	# not needed on windows
	set(M_LIBRARY "")
endif()


# once we decide how to handle the bsd daemon example, do the
# same thing for all of these:
CHECK_LIBRARY_EXISTS(socket socket "" HAVE_SOCKETLIB)
CHECK_LIBRARY_EXISTS(nsl gethostbyaddr "" HAVE_NSLIB)
CHECK_LIBRARY_EXISTS(network socket "" HAVE_NETWORKSOCKETLIB)
CHECK_LIBRARY_EXISTS(c mallopt "" HAVE_CMALLOC_LIB)
if (NOT HAVE_CMALLOC_LIB)
	CHECK_LIBRARY_EXISTS(malloc mallopt "" HAVE_MALLOC_LIB)
endif (NOT HAVE_CMALLOC_LIB)
CHECK_LIBRARY_EXISTS(mx matherr "" HAVE_LIBMX)
CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_DLOPEN)
CHECK_LIBRARY_EXISTS(l yyless "" HAVE_SOLARISLEXERLIB)
CHECK_LIBRARY_EXISTS(stdc++ main "" HAVE_STDCCXX)



# Not sure if this is correct or not!
BRLCAD_INCLUDE_FILE(stddef.h STDC_HEADERS)

# Some of these include should probably be handled by the Find*
# routines - currently this isn't quite a 1/1 match to the list
# in configure.ac and will probably change more - will also need to
# make sure local variations of the Find* routines do what is needed
# if CMake default Modules don't.
BRLCAD_INCLUDE_FILE(alloca.h HAVE_ALLOCA_H)
BRLCAD_INCLUDE_FILE(dslib.h HAVE_DSLIB_H)
BRLCAD_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
BRLCAD_INCLUDE_FILE(getopt.h HAVE_GETOPT_H)
BRLCAD_INCLUDE_FILE(gl/device.h HAVE_GL_DEVICE_H)
BRLCAD_INCLUDE_FILE(grp.h HAVE_GRP_H)
BRLCAD_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
BRLCAD_INCLUDE_FILE(io.h HAVE_IO_H)
BRLCAD_INCLUDE_FILE(libgen.h HAVE_LIBGEN_H)
BRLCAD_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
BRLCAD_INCLUDE_FILE(ncurses.h HAVE_NCURSES_H)
BRLCAD_INCLUDE_FILE(netinet/in.h HAVE_NETINET_IN_H)
BRLCAD_INCLUDE_FILE(poll.h HAVE_POLL_H)
BRLCAD_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
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
BRLCAD_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)
BRLCAD_INCLUDE_FILE(sys/prctl.h HAVE_SYS_PRCTL_H)
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
BRLCAD_INCLUDE_FILE(tinfo.h HAVE_TINFO_H)
BRLCAD_INCLUDE_FILE(ulocks.h HAVE_ULOCKS_H)
BRLCAD_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
BRLCAD_INCLUDE_FILE(winsock.h HAVE_WINSOCK_H)
BRLCAD_INCLUDE_FILE(X11/Xlib.h HAVE_X11_XLIB_H)

# C++
BRLCAD_INCLUDE_FILE_CXX(limits HAVE_LIMITS)
BRLCAD_INCLUDE_FILE_CXX(istream HAVE_ISTREAM)

BRLCAD_FUNCTION_EXISTS(XQueryExtension HAVE_XQUERYEXTENSION)
BRLCAD_FUNCTION_EXISTS(alarm HAVE_ALARM)
BRLCAD_FUNCTION_EXISTS(alloca HAVE_ALLOCA)
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


BRLCAD_TYPE_SIZE("ssize_t" SSIZE_T)

INCLUDE(CheckCSourceRuns)

# Check for basename and dirname
CHECK_LIBRARY_EXISTS(gen basename "" HAVE_LIBGEN)
if(HAVE_LIBGEN)
	SET(basename_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)basename(argv[0]);
	return 0;
	}")
	SET(dirname_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)dirname(argv[0]);
	return 0;
	}")
ELSE(HAVE_LIBGEN_H)
	SET(basename_SRC "
	int main(int argc, char *argv[]) {
	(void)basename(argv[0]);
	return 0;
	}")
	SET(dirname_SRC "
	int main(int argc, char *argv[]) {
	(void)dirname(argv[0]);
	return 0;
	}")
ENDIF(HAVE_LIBGEN)

CHECK_C_SOURCE_RUNS("${basename_SRC}" HAVE_BASENAME)
CHECK_C_SOURCE_RUNS("${dirname_SRC}" HAVE_DIRNAME)
if (HAVE_BASENAME)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_BASENAME 1\n")
endif()
if (HAVE_DIRNAME)
	FILE(APPEND  ${CONFIG_H_FILE} "#define HAVE_DIRNAME 1\n")
endif()



# Only look for frameworks if we're on an Apple machine - start
# with the example from the cmake wiki, but will very likely need
# fleshing out
if (APPLE)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindCarbon.cmake)
	INCLUDE(${BRLCAD_CMAKE_DIR}/FindCocoa.cmake)
endif (APPLE)

# This is the top level control property for handling library building 
set(BRLCAD-BUILD_SRC_OTHER "Auto" CACHE STRING
	"Set build type" FORCE)
set_property(CACHE BRLCAD-BUILD_SRC_OTHER PROPERTY STRINGS
	"Yes" "No" "Auto")

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
INCLUDE(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)


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
OPTION(BRLCAD_SYSTEM_LIBS_ONLY "Use the system's libraries." OFF)
OPTION(BRLCAD_BUILD_LOCAL_LIBS "Build all the local copies of libraries." OFF)
IF(BRLCAD_BUILD_LOCAL_LIBS)
	SET(BRLCAD_SYSTEM_LIBS_ONLY OFF CACHE BOOL "Use the system's libraries." FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_LIBS)

# zlib Library
THIRD_PARTY_OPTION(ZLIB)
THIRD_PARTY_SUBDIR(ZLIB src/other/libz)
IF(BRLCAD_BUILD_LOCAL_ZLIB)
	SET(BRLCAD_ZLIB_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libz" CACHE STRING "Directory containing zlib headers" FORCE)
	SET(ZLIB_INCLUDE_DIR "${BRLCAD_ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
	SET(BRLCAD_ZLIB_LIBRARY "zlib" CACHE STRING "zlib" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_ZLIB)
	SET(BRLCAD_ZLIB_INCLUDE_DIR "${ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
	SET(BRLCAD_ZLIB_LIBRARY "${ZLIB_LIBRARY}" CACHE STRING "zlib library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_ZLIB)

# libtermlib Library
THIRD_PARTY_OPTION(TERMLIB)
THIRD_PARTY_SUBDIR(TERMLIB src/other/libtermlib)
IF(BRLCAD_BUILD_LOCAL_TERMLIB)
	SET(BRLCAD_TERMLIB_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libtermlib" CACHE STRING "Directory containing termlib headers" FORCE)
	SET(TERMLIB_INCLUDE_DIR "${BRLCAD_TERMLIB_INCLUDE_DIR}" CACHE STRING "Directory containing termlib headers" FORCE)
	SET(BRLCAD_TERMLIB_LIBRARY "termlib" CACHE STRING "termlib" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_TERMLIB)
	SET(BRLCAD_TERMLIB_INCLUDE_DIR "${TERMLIB_INCLUDE_DIR}" CACHE STRING "Directory containing termlib headers" FORCE)
	SET(BRLCAD_TERMLIB_LIBRARY "${TERMLIB_LIBRARY}" CACHE STRING "termlib library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_TERMLIB)



# libpng Library - Checks for ZLIB, so need to restore BRLCAD_ZLIB* settings - a
# successful search by FindZLIB will reset the basic variables.  If not using local
# copy of zlib, FindZLIB results will be identical in both cases so there is no
# danger of harming the libpng setup by changing BRLCAD_ZLIB*.
THIRD_PARTY_OPTION(PNG)
SET(ZLIB_INCLUDE_DIR "${BRLCAD_ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
SET(ZLIB_LIBRARY "${BRLCAD_ZLIB_LIBRARY}" CACHE STRING "zlib library" FORCE)
# BRL-CAD needs PNG's IO - turn it on (i.e. turn off the disabling flags)
SET(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_CONSOLE_IO)
SET(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_STDIO)
THIRD_PARTY_SUBDIR(PNG src/other/libpng)
IF(BRLCAD_BUILD_LOCAL_PNG)
	SET(BRLCAD_PNG_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libpng" CACHE STRING "Directory containing libpng headers" FORCE)
	# PNG_LIB_NAME doesn't automatically propogate to toplevel due to scoping of CMake variables - 
	# use get_directory_property to recover the value and place it in the CACHE
	get_directory_property(BRLCAD_PNG_LIB_NAME DIRECTORY src/other/libpng DEFINITION PNG_LIB_NAME)
	SET(BRLCAD_PNG_LIB_NAME "${BRLCAD_PNG_LIB_NAME}" CACHE STRING "libpng name for targets" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_PNG)
	SET(BRLCAD_PNG_INCLUDE_DIR "${PNG_PNG_INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers" FORCE)
	SET(BRLCAD_PNG_LIB_NAME "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_PNG)

# libregex Library
THIRD_PARTY_OPTION(REGEX)
THIRD_PARTY_SUBDIR(REGEX src/other/libregex)
IF(BRLCAD_BUILD_LOCAL_REGEX)
	SET(BRLCAD_REGEX_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libregex" CACHE STRING "Directory containing libregex headers" FORCE)
	SET(REGEX_INCLUDE_DIR "${BRLCAD_REGEX_INCLUDE_DIR}" CACHE STRING "Directory containing libregex headers" FORCE)
	SET(BRLCAD_REGEX_LIBRARY "regex" CACHE STRING "regex" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_REGEX)
	SET(BRLCAD_REGEX_INCLUDE_DIR "${REGEX_INCLUDE_DIR}" CACHE STRING "Directory containing libregex headers" FORCE)
	SET(BRLCAD_REGEX_LIBRARY "${REGEX_LIBRARY}" CACHE STRING "libregex library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_REGEX)

# libutahrle Library
THIRD_PARTY_OPTION(UTAHRLE)
THIRD_PARTY_SUBDIR(UTAHRLE src/other/libutahrle)
IF(BRLCAD_BUILD_LOCAL_UTAHRLE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "Directory containing libutahrle headers" FORCE)
	SET(UTAHRLE_INCLUDE_DIR "${BRLCAD_UTAHRLE_INCLUDE_DIR}" CACHE STRING "Directory containing libutahrle headers" FORCE)
	SET(BRLCAD_UTAHRLE_LIBRARY "utahrle" CACHE STRING "libutahrle" FORCE)
	SET(UTAHRLE_LIBRARY "utahrle" CACHE STRING "libutahrle" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_UTAHRLE)
	SET(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "Directory containing libutahrle headers" FORCE)
	SET(BRLCAD_UTAHRLE_LIBRARY "${UTAHRLE_LIBRARY}" CACHE STRING "libutahrle library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_UTAHRLE)

# URTToolkit
# Programs, not a library - needs libutahrle, but the "check for system install" logic
# will be different here.  Ignore for now, revisit later.
ADD_SUBDIRECTORY(src/other/URToolkit)



# Tcl/Tk presents a number of complexities for BRL-CAD and CMake - BRL-CAD requires
# far more knowledge of the details of a Tcl/Tk installation than most programs,
# which unfortunately means the standard FindTCL.cmake is not sufficient.  Rather than
# use THIRD_PARTY_OPTION here, we will instead provide the option manually and ensure
# that BRL-CAD's own FindTCL.cmake file is loaded.  

include(ExternalProject)

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
SET(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
SET(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)
THIRD_PARTY_OPTION(TCL)
IF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" ON)
ELSE(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
	OPTION(BRLCAD_BUILD_LOCAL_TK "Build the local versions of Tcl/Tk" OFF)
ENDIF(BRLCAD_BUILD_LOCAL_TCL AND TCL_REQUIRE_TK)
IF(NOT BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_SYSTEM_LIBS_ONLY)
	SET(TCL_DEP "")
	SET(TK_DEP "")
	ExternalProject_Add(
		tcl
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tcl/unix
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
	ExternalProject_Add(
		tk
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tk/unix
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_SYSTEM_LIBS_ONLY)
IF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_BUILD_LOCAL_LIBS)
	SET(BRLCAD_BUILD_LOCAL_TK ON)
	
	ExternalProject_Add(
		tcl
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tcl/unix
		CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/tcl/unix -o
		${BRLCAD_SOURCE_DIR}/src/other/tcl/unix/configure
		${BRLCAD_SOURCE_DIR}/src/other/tcl/unix/configure.in && mkdir -p
		${BRLCAD_BINARY_DIR}/src/other/tcl/ && cd
		${BRLCAD_BINARY_DIR}/src/other/tcl/ &&
		<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --mandir=${BRLCAD_INSTALL_MAN_DIR}
		BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/tcl/ && $(MAKE)
		INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/tcl/ && $(MAKE) install
		)
	SET(TCL_LIBRARY "${BRLCAD_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tcl8.5${CMAKE_SHARED_LIBRARY_SUFFIX}")
	SET(TCL_INCLUDE_PATH "${BRLCAD_PREFIX}/include")
	SET(TCL_PRIVATE_HDRS "${BRLCAD_SOURCE_DIR}/src/other/tcl/generic;${BRLCAD_SOURCE_DIR}/src/other/tcl/unix")
	SET(TCL_CONF_PREFIX "${BRLCAD_PREFIX}/lib")
	MESSAGE("TCL_LIBRARY: ${TCL_LIBRARY}")
	SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};tcl")

	ExternalProject_Add(
		tk
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tk/unix
		CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/tk/unix -o
		${BRLCAD_SOURCE_DIR}/src/other/tk/unix/configure
		${BRLCAD_SOURCE_DIR}/src/other/tk/unix/configure.in && mkdir -p
		${BRLCAD_BINARY_DIR}/src/other/tk/ && cd
		${BRLCAD_BINARY_DIR}/src/other/tk/ &&
		<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --with-tcl=${BRLCAD_PREFIX}/lib --mandir=${BRLCAD_INSTALL_MAN_DIR}
		BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/tk/ && $(MAKE)
		INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/tk/ && $(MAKE) install
		)
	SET(TK_LIBRARY "${BRLCAD_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tk8.5${CMAKE_SHARED_LIBRARY_SUFFIX}")
	SET(TK_INCLUDE_PATH "${BRLCAD_PREFIX}/include")
	SET(TK_PRIVATE_HDRS "${BRLCAD_SOURCE_DIR}/src/other/tk/generic;${BRLCAD_SOURCE_DIR}/src/other/tk/unix")
	SET(TK_CONF_PREFIX "${BRLCAD_PREFIX}/lib")
	MESSAGE("TK_LIBRARY: ${TK_LIBRARY}")
	SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};tk")
	SET(TCL_DEP "tcl")
	SET(TK_DEP "tk")
ENDIF(BRLCAD_BUILD_LOCAL_TCL OR BRLCAD_BUILD_LOCAL_LIBS)
IF(TK_LIBRARY)
	SET(HAVE_TK 1)
	FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_TK 1\n")
ENDIF(TK_LIBRARY)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
INCLUDE(${BRLCAD_CMAKE_DIR}/FindTclPackage.cmake)
IF(NOT BRLCAD_BUILD_LOCAL_TCL)
	IF(TCL_TCLSH)
		FIND_TCL_PACKAGE(${TCL_TCLSH} Itcl)
	ENDIF(TCL_TCLSH)
	IF(NOT BRLCAD_BUILD_LOCAL_TK)
		IF(TK_WISH)
			FIND_TCL_PACKAGE(${TK_WISH} Itk)
			FIND_TCL_PACKAGE(${TK_WISH} Iwidgets)
			FIND_TCL_PACKAGE(${TK_WISH} Tkhtml)
			FIND_TCL_PACKAGE(${TK_WISH} tkpng)
			FIND_TCL_PACKAGE(${TK_WISH} Tktable)
		ENDIF(TK_WISH)
	ENDIF(NOT BRLCAD_BUILD_LOCAL_TK)
ENDIF(NOT BRLCAD_BUILD_LOCAL_TCL)

# Handle Tcl/Tk packages
IF(NOT ITCL_FOUND OR NOT ITK_FOUND OR BRLCAD_BUILD_LOCAL_ITCL)
	IF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			itcl
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itcl
			CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itcl -o
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itcl/configure
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itcl/configure.in && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/itcl/ && cd
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/itcl/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX} --with-tk=${TK_CONF_PREFIX} --mandir=${BRLCAD_INSTALL_MAN_DIR}
			BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/incrTcl/itcl && $(MAKE)
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/incrTcl/itcl && $(MAKE) install
			)

	         SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};itcl")
		ExternalProject_Add(
			itk
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itk
			CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itk -o
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itk/configure
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/itk/configure.in && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/itk/ && cd
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/itk/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX} --with-tk=${TK_CONF_PREFIX} --mandir=${BRLCAD_INSTALL_MAN_DIR}
			BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/incrTcl/itk && $(MAKE)
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/incrTcl/itk && $(MAKE) install
			)
	         SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};itk")

		IF(TCL_DEP OR TK_DEP)
			ADD_DEPENDENCIES(itcl ${TCL_DEP} ${TK_DEP})
		ENDIF(TCL_DEP OR TK_DEP)
		ADD_DEPENDENCIES(itk itcl ${TCL_DEP} ${TK_DEP})
		SET(BRLCAD_BUILD_LOCAL_INCRTCL ON)
		SET(ITCL_DEP "itcl")
		SET(ITK_DEP "itk")
		SET(ITCL_VERSION "3.4")
		SET(ITK_VERSION "3.3")
	ELSE(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF)
	ENDIF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
ELSE(NOT ITCL_FOUND OR NOT ITK_FOUND OR BRLCAD_BUILD_LOCAL_ITCL)
	SET(BRLCAD_BUILD_LOCAL_INCRTCL OFF)
	SET(ITCL_VERSION ${ITCL_PACKAGE_VERSION})
	SET(ITK_VERSION ${ITK_PACKAGE_VERSION})
ENDIF(NOT ITCL_FOUND OR NOT ITK_FOUND OR BRLCAD_BUILD_LOCAL_ITCL)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")

IF(NOT IWIDGETS_FOUND OR BRLCAD_BUILD_LOCAL_IWIDGETS)
	IF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			iwidgets
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/iwidgets
			CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/iwidgets -o
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/iwidgets/configure
			${BRLCAD_SOURCE_DIR}/src/other/incrTcl/iwidgets/configure.in && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/iwidgets/ && cd
			${BRLCAD_BINARY_DIR}/src/other/incrTcl/iwidgets/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX}
			--with-tk=${TK_CONF_PREFIX} --mandir=${BRLCAD_INSTALL_MAN_DIR}
			--with-itcl=${BRLCAD_SOURCE_DIR}/src/other/incrTcl
			BUILD_COMMAND ""
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/incrTcl/iwidgets && $(MAKE) install
			)
	         SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};iwidgets")
		IF(TCL_DEP OR TK_DEP OR ITCL_DEP OR ITK_DEP)
			ADD_DEPENDENCIES(iwidgets ${TCL_DEP} ${TK_DEP} ${ITCL_DEP} ${ITK_DEP})
		ENDIF(TCL_DEP OR TK_DEP OR ITCL_DEP OR ITK_DEP)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS ON)
	ELSE(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		SET(BRLCAD_BUILD_LOCAL_IWIDGETS OFF)
	ENDIF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
ELSE(NOT IWIDGETS_FOUND OR BRLCAD_BUILD_LOCAL_IWIDGETS)
	ExternalProject_Add(
		iwidgets
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/incrTcl/iwidgets
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
	SET(BRLCAD_BUILD_LOCAL_IWIDGETS OFF)
ENDIF(NOT IWIDGETS_FOUND OR BRLCAD_BUILD_LOCAL_IWIDGETS)
FILE(APPEND ${CONFIG_H_FILE} "#define IWIDGETS_VERSION \"4.0.1\"\n")

IF(NOT TKHTML_FOUND OR BRLCAD_BUILD_LOCAL_TKHTML)
	IF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			tkhtml
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tkhtml
			CONFIGURE_COMMAND cd ${BRLCAD_SOURCE_DIR}/src/other/tkhtml && autoreconf -i -f && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/tkhtml/ && cd
			${BRLCAD_BINARY_DIR}/src/other/tkhtml/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX} --with-tk=${TK_CONF_PREFIX}
			BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/tkhtml && $(MAKE)
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/tkhtml && $(MAKE) install
			)
	        SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};tkhtml")
		IF(TCL_DEP OR TK_DEP)
			ADD_DEPENDENCIES(tkhtml ${TCL_DEP} ${TK_DEP})
		ENDIF(TCL_DEP OR TK_DEP)
		SET(BRLCAD_BUILD_LOCAL_TKHTML ON)
	ELSE(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		SET(BRLCAD_BUILD_LOCAL_TKHTML OFF)
	ENDIF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
ELSE(NOT TKHTML_FOUND OR BRLCAD_BUILD_LOCAL_TKHTML)
	ExternalProject_Add(
		tkhtml
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tkhtml
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
	SET(BRLCAD_BUILD_LOCAL_TKHTML OFF)
ENDIF(NOT TKHTML_FOUND OR BRLCAD_BUILD_LOCAL_TKHTML)

IF(NOT TKPNG_FOUND OR BRLCAD_BUILD_LOCAL_TKPNG)
	IF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			tkpng
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tkpng
			CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/tkpng -o
			${BRLCAD_SOURCE_DIR}/src/other/tkpng/configure
			${BRLCAD_SOURCE_DIR}/src/other/tkpng/configure.in && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/tkpng/ && cd
			${BRLCAD_BINARY_DIR}/src/other/tkpng/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX} --with-tk=${TK_CONF_PREFIX}
			BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/tkpng && $(MAKE)
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/tkpng && $(MAKE) install
			)
	        SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};tkpng")
		IF(TCL_DEP OR TK_DEP)
			ADD_DEPENDENCIES(tkpng ${TCL_DEP} ${TK_DEP})
		ENDIF(TCL_DEP OR TK_DEP)
		SET(BRLCAD_BUILD_LOCAL_TKPNG ON)
	ELSE(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		SET(BRLCAD_BUILD_LOCAL_TKPNG OFF)
	ENDIF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
ELSE(NOT TKPNG_FOUND OR BRLCAD_BUILD_LOCAL_TKPNG)
	ExternalProject_Add(
		tkpng
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tkpng
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
	SET(BRLCAD_BUILD_LOCAL_TKPNG OFF)
ENDIF(NOT TKPNG_FOUND OR BRLCAD_BUILD_LOCAL_TKPNG)

IF(NOT TKTABLE_FOUND OR BRLCAD_BUILD_LOCAL_TKTABLE)
	IF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		ExternalProject_Add(
			tktable
			DOWNLOAD_COMMAND ""
			PREFIX ${BRLCAD_BINARY_DIR}
			SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tktable
			CONFIGURE_COMMAND autoconf -I ${BRLCAD_SOURCE_DIR}/src/other/tktable -o
			${BRLCAD_SOURCE_DIR}/src/other/tktable/configure
			${BRLCAD_SOURCE_DIR}/src/other/tktable/configure.in && mkdir -p
			${BRLCAD_BINARY_DIR}/src/other/tktable/ && cd
			${BRLCAD_BINARY_DIR}/src/other/tktable/ &&
			<SOURCE_DIR>/configure --prefix=${BRLCAD_PREFIX} --exec-prefix=${BRLCAD_PREFIX}
			--with-tcl=${TCL_CONF_PREFIX} --with-tk=${TK_CONF_PREFIX}
			BUILD_COMMAND cd ${BRLCAD_BINARY_DIR}/src/other/tktable && $(MAKE)
			INSTALL_COMMAND  cd ${BRLCAD_BINARY_DIR}/src/other/tktable && $(MAKE) install
			)
	        SET(CMAKE_EXTERNAL_TARGET_LIST "${CMAKE_EXTERNAL_TARGET_LIST};tktable")
		IF(TCL_DEP OR TK_DEP)
			ADD_DEPENDENCIES(tktable ${TCL_DEP} ${TK_DEP})
		ENDIF(TCL_DEP OR TK_DEP)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE ON)
	ELSE(NOT BRLCAD_SYSTEM_LIBS_ONLY)
		SET(BRLCAD_BUILD_LOCAL_TKTABLE OFF)
	ENDIF(NOT BRLCAD_SYSTEM_LIBS_ONLY)
ELSE(NOT TKTABLE_FOUND OR BRLCAD_BUILD_LOCAL_TKTABLE)
	ExternalProject_Add(
		tktable
		DOWNLOAD_COMMAND ""
		PREFIX ${BRLCAD_BINARY_DIR}
		SOURCE_DIR ${BRLCAD_SOURCE_DIR}/src/other/tktable
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND "" 
		)
	SET(BRLCAD_BUILD_LOCAL_TKTABLE OFF)
ENDIF(NOT TKTABLE_FOUND OR BRLCAD_BUILD_LOCAL_TKTABLE)



# OpenNURBS Library
THIRD_PARTY_OPTION(OPENNURBS)
THIRD_PARTY_SUBDIR(OPENNURBS src/other/openNURBS)
IF(BRLCAD_BUILD_LOCAL_OPENNURBS)
	SET(BRLCAD_OPENNURBS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Directory containing openNURBS headers" FORCE)
	SET(OPENNURBS_INCLUDE_DIR "${BRLCAD_OPENNURBS_INCLUDE_DIR}" CACHE STRING "Directory containing openNURBS headers" FORCE)
	SET(BRLCAD_OPENNURBS_LIBRARY "openNURBS" CACHE STRING "openNURBS" FORCE)
	BRLCAD_INCLUDE_FILE(emmintrin.h HAVE_EMMINTRIN_H)
ELSE(BRLCAD_BUILD_LOCAL_OPENNURBS)
	SET(BRLCAD_OPENNURBS_INCLUDE_DIR "${OPENNURBS_INCLUDE_DIR}" CACHE STRING "Directory containing openNURBS headers" FORCE)
	SET(BRLCAD_OPENNURBS_LIBRARY "${OPENNURBS_LIBRARY}" CACHE STRING "openNURBS library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_OPENNURBS)


# STEP Class Libraries
THIRD_PARTY_OPTION(SCL)
THIRD_PARTY_SUBDIR(SCL src/other/step)
IF(BRLCAD_BUILD_LOCAL_SCL)
	SET(BRLCAD_SCL_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/step/include" CACHE STRING "Directory containing SCL headers" FORCE)
	SET(BRLCAD_SCL_EXPRESS_EXECUTABLE "express" CACHE STRING "SCL express binary" FORCE)
	SET(BRLCAD_SCL_SYMLINK_EXECUTABLE "symlink" CACHE STRING "SCL symlink binary" FORCE)
	SET(BRLCAD_SCL_EXPPP_EXECUTABLE "exppp" CACHE STRING "SCL exppp binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_OS_EXECUTABLE "fedex_os" CACHE STRING "SCL fedex_os binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_IDL_EXECUTABLE "fedex_idl" CACHE STRING "SCL fedex_idl binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_PLUS_EXECUTABLE "fedex_plus" CACHE STRING "SCL fedex_plus binary" FORCE)
	SET(BRLCAD_SCL_EXPPP_LIB "exppplib" CACHE STRING "SCL exppp library" FORCE)
	SET(BRLCAD_SCL_CORE_LIB "stepcore" CACHE STRING "SCL core library" FORCE)
	SET(BRLCAD_SCL_UTILS_LIB "steputils" CACHE STRING "SCL utilities library" FORCE)
	SET(BRLCAD_SCL_DAI_LIB "stepdai" CACHE STRING "SCL DAI library" FORCE)
	SET(BRLCAD_SCL_EDITOR_LIB "stepeditor" CACHE STRING "SCL editor library" FORCE)
ELSE(BRLCAD_BUILD_LOCAL_SCL)
	SET(BRLCAD_SCL_INCLUDE_DIR "${SCL_INCLUDE_DIR}" CACHE STRING "Directory containing SCL headers" FORCE)
	SET(BRLCAD_SCL_EXPRESS_EXECUTABLE "${SCL_EXPRESS_EXECUTABLE}" CACHE STRING "SCL express binary" FORCE)
	SET(BRLCAD_SCL_SYMLINK_EXECUTABLE "${SCL_SYMLINK_EXECUTABLE}" CACHE STRING "SCL symlink binary" FORCE)
	SET(BRLCAD_SCL_EXPPP_EXECUTABLE "${SCL_EXPPP_EXECUTABLE}" CACHE STRING "SCL exppp binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_OS_EXECUTABLE "${SCL_FEDEX_OS_EXECUTABLE}" CACHE STRING "SCL fedex_os binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_IDL_EXECUTABLE "${SCL_FEDEX_IDL_EXECUTABLE}" CACHE STRING "SCL fedex_idl binary" FORCE)
	SET(BRLCAD_SCL_FEDEX_PLUS_EXECUTABLE "${SCL_FEDEX_PLUS_EXECUTABLE}" CACHE STRING "SCL fedex_plus binary" FORCE)
	SET(BRLCAD_SCL_EXPPP_LIB "${SCL_EXPPP_LIB}" CACHE STRING "SCL exppp library" FORCE)
	SET(BRLCAD_SCL_CORE_LIB "${SCL_CORE_LIB}" CACHE STRING "SCL core library" FORCE)
	SET(BRLCAD_SCL_UTILS_LIB "${SCL_UTILS_LIB}" CACHE STRING "SCL utilities library" FORCE)
	SET(BRLCAD_SCL_DAI_LIB "${SCL_DAI_LIB}" CACHE STRING "SCL DAI library" FORCE)
	SET(BRLCAD_SCL_EDITOR_LIB "${SCL_EDITOR_LIB}" CACHE STRING "SCL editor library" FORCE)
ENDIF(BRLCAD_BUILD_LOCAL_SCL)


# Now that all the tests are done, configure the brlcad_config.h file:
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)

# Virtually all of the BRL-CAD code needs the BRL-CAD include directories
# and a couple of standard defines - set those up in variables now - the
# idea being that if any other system-wide defines or include are needed
# they can just be added here.
SET(BRLCAD_STD_DEFS "-DHAVE_CONFIG_H;-DBRLCADBUILD=1")
SET(BRLCAD_STD_INCLUDES "${BRLCAD_BINARY_DIR}/include;${BRLCAD_SOURCE_DIR}/include")

ADD_SUBDIRECTORY(src/libbu)
ADD_SUBDIRECTORY(src/libbn)
ADD_SUBDIRECTORY(src/libsysv)
ADD_SUBDIRECTORY(src/librt)
#ADD_SUBDIRECTORY(src/external)
ADD_SUBDIRECTORY(src/libpkg)
ADD_SUBDIRECTORY(src/libfb)
ADD_SUBDIRECTORY(src/libwdb)
ADD_SUBDIRECTORY(src/libgcv)
ADD_SUBDIRECTORY(src/libanalyze)
ADD_SUBDIRECTORY(src/libged)
ADD_SUBDIRECTORY(src/liboptical)
ADD_SUBDIRECTORY(src/liborle)
ADD_SUBDIRECTORY(src/libcursor)
ADD_SUBDIRECTORY(src/conv)
ADD_SUBDIRECTORY(src/fbserv)
ADD_SUBDIRECTORY(src/rt)
#ADD_SUBDIRECTORY(src/librtserver)
ADD_SUBDIRECTORY(src/libdm)
ADD_SUBDIRECTORY(src/libfft)
ADD_SUBDIRECTORY(src/libicv)
ADD_SUBDIRECTORY(src/libmultispectral)
ADD_SUBDIRECTORY(src/libtclcad)
ADD_SUBDIRECTORY(src/libtermio)
ADD_SUBDIRECTORY(src/adrt)
ADD_SUBDIRECTORY(src/anim)
ADD_SUBDIRECTORY(src/brlman)
ADD_SUBDIRECTORY(src/burst)
ADD_SUBDIRECTORY(src/bwish)
ADD_SUBDIRECTORY(src/canon)
ADD_SUBDIRECTORY(src/fb)
ADD_SUBDIRECTORY(src/vfont)
ADD_SUBDIRECTORY(src/fbed)
ADD_SUBDIRECTORY(src/gtools)
ADD_SUBDIRECTORY(src/halftone)
ADD_SUBDIRECTORY(src/irprep)
ADD_SUBDIRECTORY(src/lgt)
ADD_SUBDIRECTORY(src/mged)
ADD_SUBDIRECTORY(src/nirt)
ADD_SUBDIRECTORY(src/proc-db)
ADD_SUBDIRECTORY(src/remrt)
ADD_SUBDIRECTORY(src/rttherm)
ADD_SUBDIRECTORY(src/shapes)
ADD_SUBDIRECTORY(src/sig)
ADD_SUBDIRECTORY(src/tab)
#ADD_SUBDIRECTORY(src/tclscripts)
ADD_SUBDIRECTORY(src/util)
ADD_SUBDIRECTORY(src/vdeck)
ADD_SUBDIRECTORY(db)

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
	SET(BRLCAD-ENABLE_RTSERVERJDK_LABEL "librtserver JDK support (optional) ")
	SET(BRLCAD-ENABLE_RUNTIMEDEBUGGING_LABEL "Enable run-time debugging (optional) ")
	if (BRLCAD-ENABLE_64BIT)
		set(BRLCAD_ARCH_BITSETTING "ON")
	else (BRLCAD-ENABLE_64BIT)
		set(BRLCAD_ARCH_BITSETTING "OFF (32 bit)")
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
	SET(BRLCAD-BUILD_BENCHMARK_ONLY_LABEL "Only build benchmark suite ")
	SET(BRLCAD-BUILD_RTSERVER_ONLY_LABEL "Only build librtserver ")
	SET(BRLCAD-BUILD_GEOMETRY_LABEL "Install example geometry models ")
	if (BRLCAD-BUILD_EXTRADOCS)
		if (BRLCAD-BUILD_PDF_DOCS)
			set(BRLCAD-BUILD_DOCBUILD_MSG "ON (man/html/pdf)")
		else (BRLCAD-BUILD_PDF_DOCS)
			set(BRLCAD-BUILD_DOCBUILD_MSG "ON (man/html only)")
		endif (BRLCAD-BUILD_PDF_DOCS)
	else (BRLCAD-BUILD_EXTRADOCS)
		set(BRLCAD-BUILD_DOCBUILD_MSG "OFF")
	endif (BRLCAD-BUILD_EXTRADOCS)
	SET(BRLCAD-BUILD_DOCBUILD_MSG_LABEL "Build extra docs ")

	SET(SETTINGLIST1 BRLCAD_BUILD_LOCAL_TCL BRLCAD_BUILD_LOCAL_TK BRLCAD_BUILD_LOCAL_INCRTCL BRLCAD_BUILD_LOCAL_IWIDGETS BRLCAD_BUILD_LOCAL_TKHTML BRLCAD_BUILD_LOCAL_TKPNG BRLCAD_BUILD_LOCAL_TKTABLE BRLCAD_BUILD_LOCAL_PNG BRLCAD_BUILD_LOCAL_REGEX BRLCAD_BUILD_LOCAL_ZLIB BRLCAD_BUILD_LOCAL_TERMLIB BRLCAD_BUILD_LOCAL_UTAHRLE BRLCAD_BUILD_LOCAL_OPENNURBS BRLCAD_BUILD_LOCAL_SCL)
	SET(SETTINGLIST2 BRLCAD-ENABLE_X11 BRLCAD-ENABLE_OPENGL BRLCAD-ENABLE_RTSERVERJDK BRLCAD-ENABLE_RUNTIMEDEBUGGING )
	SET(SETTINGLIST3 BRLCAD_ARCH_BITSETTING BRLCAD-ENABLE_OPTIMIZED BRLCAD-ENABLE_DEBUG BRLCAD-ENABLE_PROFILE BRLCAD-ENABLE_SMP BRLCAD-ENABLE_STATICLIBS BRLCAD-ENABLE_DYNAMICLIBS BRLCAD-ENABLE_VERBOSEWARN BRLCAD-ENABLE_VERBOSEPROGRESS BRLCAD-BUILD_BENCHMARK_ONLY BRLCAD-BUILD_RTSERVER_ONLY BRLCAD-BUILD_GEOMETRY BRLCAD-BUILD_DOCBUILD_MSG)
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

# We want the timestamp to come first, so make ALL library,
# exec and external targets depend on timestamp (except for
# the exec target printtimestamp, which is needed by timestamp)
FOREACH(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
  if(libtarget)
    ADD_DEPENDENCIES(${libtarget} timestamp)
  endif(libtarget)
ENDFOREACH(libtarget ${CMAKE_LIBRARY_TARGET_LIST})
FOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
  if(exectarget)
    IF(NOT ${exectarget} MATCHES "printtimestamp")
      ADD_DEPENDENCIES(${exectarget} timestamp)
    ENDIF(NOT ${exectarget} MATCHES "printtimestamp")
  endif(exectarget)
ENDFOREACH(exectarget ${CMAKE_EXEC_TARGET_LIST})
FOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})
  if(externaltarget)
    ADD_DEPENDENCIES(${externaltarget} timestamp)
  endif(externaltarget)
ENDFOREACH(externaltarget ${CMAKE_EXTERNAL_TARGET_LIST})


if(BRLCAD_BUILD_LOCAL_TCL)
  ADD_DEPENDENCIES(tk tcl)
  ADD_DEPENDENCIES(bu ${TCL_DEP} ${TK_DEP})
  ADD_DEPENDENCIES(sysv ${TCL_DEP} ${TK_DEP})
  ADD_DEPENDENCIES(librt ${TCL_DEP} ${TK_DEP})
endif(BRLCAD_BUILD_LOCAL_TCL)




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



# Generate the include/conf files

# HOST 
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_hostname.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/report_hostname.c
	OUTPUT_VARIABLE COMPILEMESSAGES)

# USER
configure_file(${BRLCAD_CMAKE_DIR}/test_srcs/report_username.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c)
TRY_RUN(TIME_RESULT TIME_COMPILED 
	${CMAKE_BINARY_DIR}/CMakeTmp
	${CMAKE_BINARY_DIR}/CMakeTmp/report_username.c
	OUTPUT_VARIABLE COMPILEMESSAGES)

# PATH
FILE(WRITE ${BRLCAD_BINARY_DIR}/include/conf/PATH "\"${BRLCAD_PREFIX}\"") 

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

# DATE
file(READ ${CONFIG_TIMESTAMP_FILE} DATESTAMP)
STRING(STRIP ${DATESTAMP} DATESTAMP)
file(WRITE ${BRLCAD_BINARY_DIR}/include/conf/DATE "\"${DATESTAMP}\"")

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
