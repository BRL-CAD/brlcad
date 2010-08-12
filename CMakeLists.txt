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
# etc.  There are several approaches to this - the most theoretically robust,
# and the first one tried, builds a small C file which writes out the needed
# values to files in the build directory, which are then read and stripped by
# CMake - essentially the same approach as the version numbers except with the
# C file generating the files.  If that fails, fall back on command line scripts
# and output capture - this will work well on systems with the date command but
# Windows does not provide region-independent ordering for day/month in its
# command line tool outputs.  It is vastly preferable to have the C based
# solution succeed. 
SET(CONFIG_TIME_DAY_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_DAY")
SET(CONFIG_TIME_MONTH_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_MONTH")
SET(CONFIG_TIME_YEAR_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIME_YEAR")
SET(CONFIG_TIMESTAMP_FILE "${BRLCAD_BINARY_DIR}/include/conf/CONFIG_TIMESTAMP")
FILE(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include")
FILE(MAKE_DIRECTORY "${BRLCAD_BINARY_DIR}/include/conf")
configure_file(${CONFIG_H_FILE} ${BRLCAD_CMAKE_DIR}/test_srcs/time.c.in ${CMAKE_BINARY_DIR}/CMakeTmp/time.c)
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
   IF(WIN32)
      # There appears to be no way to spot the region-specific aspsects of this reliably - regionalize
      # as appropriate to your computer, or if there is a command-line way to access this information
      # on Windows please enhance this logic.
      execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE CONFIG_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
      execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE CONFIG_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
      SET(CONFIG_TS "${CONFIG_DATE} ${CONFIG_TIME}")
      STRING(REGEX REPLACE "[^0-9]*([0-9][0-9]).*" "\\1" CONFIG_MONTH "${CONFIG_DATE}" )
      STRING(REGEX REPLACE "[^0-9]*[0-9][0-9].([0-9][0-9]).*" "\\1" CONFIG_DAY "${CONFIG_DATE}" )
      STRING(REGEX REPLACE "[^0-9]*[0-9][0-9].[0-9][0-9].([0-9][0-9][0-9][0-9]).*" "\\1" CONFIG_YEAR "${CONFIG_DATE}" )
   ELSE(WIN32)
      EXEC_PROGRAM(date ARGS +%d OUTPUT_VARIABLE CONFIG_DAY) 
      EXEC_PROGRAM(date ARGS +%m OUTPUT_VARIABLE CONFIG_MONTH) 
      EXEC_PROGRAM(date ARGS +%Y OUTPUT_VARIABLE CONFIG_YEAR)
      SET(CONFIG_DATE "${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}")
      # try RFC 2822 for timestamp
      EXEC_PROGRAM(date ARGS -R 2>/dev/null OUTPUT_VARIABLE CONFIG_TS)
      IF ("${CONFIG_TS}" MATCHES ".*usage.*")
         EXEC_PROGRAM(date ARGS +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null OUTPUT_VARIABLE CONFIG_TS) 
      ENDIF ("${CONFIG_TS}" MATCHES ".*usage.*")
      IF ("${CONFIG_TS}" MATCHES ".*usage.*")
         EXEC_PROGRAM(date ARGS OUTPUT_VARIABLE CONFIG_TS) 
      ENDIF ("${CONFIG_TS}" MATCHES ".*usage.*")
   ENDIF(WIN32)
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
MESSAGE(STATUS "Install directory prefix: ${BRLCAD_PREFIX}")

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
   find_package(X11)
endif (BRLCAD-ENABLE_X11)

OPTION(BRLCAD-ENABLE_OPENGL "Use OpenGL." ON)
if (BRLCAD-ENABLE_OPENGL)
   INCLUDE(${BRLCAD_CMAKE_DIR}/FindGL.cmake)
endif (BRLCAD-ENABLE_OPENGL)

INCLUDE (CheckLibraryExists)

# This may be overkill - does CHECK_LIBRARY_EXISTS usably set
# anything for us?  May want to wrap some combination of
# CHECK_LIBRARY_EXISTS and FIND_LIBRARY into a macro...
set(LIBBSD "")
CHECK_LIBRARY_EXISTS(bsd daemon "" HAVE_BSDDAEMON)
if (NOT HAVE_BSDDAEMON)
   CHECK_LIBRARY_EXISTS(c daemon "" HAVE_CDAEMON)
endif (NOT HAVE_BSDDAEMON)
if (HAVE_BSDDAEMON)
   set(LIBBSD "-lbsd")
endif (HAVE_BSDDAEMON)
if (HAVE_CDAEMON)
   set(LIBBSD "-c")
endif (HAVE_CDAEMON)

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

FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/COUNT "1")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/DATE "\"${CONFIG_TS}\"")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/HOST "\"some_machine\"")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/PATH "\"${BRLCAD_PREFIX}\"")
FILE(WRITE ${BRLCAD_SOURCE_DIR}/include/conf/USER "\"some_user\"")

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

# libtermlib Library
BRLCAD_THIRD_PARTY_OPTION(LIBTERM termlib)
BRLCAD_THIRD_PARTY_SUBDIR(LIBTERM src/other/libtermlib)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_LIBTERM ON CACHE BOOL "Use the system's termlib library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_LIBTERM)
   SET(BRLCAD_LIBTERM_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libtermlib" CACHE STRING "Directory containing termlib headers" FORCE)
   SET(LIBTERM_INCLUDE_DIR "${BRLCAD_LIBTERM_INCLUDE_DIR}" CACHE STRING "Directory containing termlib headers" FORCE)
   SET(BRLCAD_LIBTERM_LIBRARY "termlib" CACHE STRING "termlib" FORCE)
ELSE(NOT BRLCAD_USE_SYSTEM_LIBTERM)
   SET(BRLCAD_LIBTERM_INCLUDE_DIR "${LIBTERM_INCLUDE_DIR}" CACHE STRING "Directory containing termlib headers" FORCE)
   SET(BRLCAD_LIBTERM_LIBRARY "${LIBTERM_LIBRARY}" CACHE STRING "termlib library" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_LIBTERM)



# libpng Library - Checks for ZLIB, so need to restore BRLCAD_ZLIB* settings - a
# successful search by FindZLIB will reset the basic variables.  If not using local
# copy of zlib, FindZLIB results will be identical in both cases so there is no
# danger of harming the libpng setup by changing BRLCAD_ZLIB*.
BRLCAD_THIRD_PARTY_OPTION(PNG libpng)
SET(ZLIB_INCLUDE_DIR "${BRLCAD_ZLIB_INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers" FORCE)
SET(ZLIB_LIBRARY "${BRLCAD_ZLIB_LIBRARY}" CACHE STRING "zlib library" FORCE)
# BRL-CAD needs PNG's IO - turn it on (i.e. turn off the disabling flags)
SET(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_CONSOLE_IO)
SET(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
MARK_AS_ADVANCED(PNG_NO_STDIO)
BRLCAD_THIRD_PARTY_SUBDIR(PNG src/other/libpng)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_PNG ON CACHE BOOL "Use the system's libpng library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_PNG)
   SET(BRLCAD_PNG_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libpng" CACHE STRING "Directory containing libpng headers" FORCE)
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
   SET(BRLCAD_REGEX_LIBRARY "regex" CACHE STRING "regex" FORCE)
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

# OpenNURBS Library
BRLCAD_THIRD_PARTY_OPTION(OPENNURBS openNURBS)
BRLCAD_THIRD_PARTY_SUBDIR(OPENNURBS src/other/openNURBS)
IF(BRLCAD_USE_SYSTEM_LIBS)
   SET(BRLCAD_USE_SYSTEM_OPENNURBS ON CACHE BOOL "Use the system's openNURBS library" FORCE)
ENDIF(BRLCAD_USE_SYSTEM_LIBS)
IF(NOT BRLCAD_USE_SYSTEM_OPENNURBS)
   SET(BRLCAD_OPENNURBS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Directory containing openNURBS headers" FORCE)
   SET(OPENNURBS_INCLUDE_DIR "${BRLCAD_OPENNURBS_INCLUDE_DIR}" CACHE STRING "Directory containing openNURBS headers" FORCE)
   SET(BRLCAD_OPENNURBS_LIBRARY "openNURBS" CACHE STRING "openNURBS" FORCE)
   BRLCAD_INCLUDE_FILE(emmintrin.h HAVE_EMMINTRIN_H)
ELSE(NOT BRLCAD_USE_SYSTEM_OPENNURBS)
   SET(BRLCAD_OPENNURBS_INCLUDE_DIR "${OPENNURBS_INCLUDE_DIR}" CACHE STRING "Directory containing openNURBS headers" FORCE)
   SET(BRLCAD_OPENNURBS_LIBRARY "${OPENNURBS_LIBRARY}" CACHE STRING "openNURBS library" FORCE)
ENDIF(NOT BRLCAD_USE_SYSTEM_OPENNURBS)

# STEP is going to require some extra thought - it's a src/other 
# with multiple libraries.  For now, just add it as a subdir
ADD_SUBDIRECTORY(src/other/step)


# Now that all the tests are done, configure the brlcad_config.h file:
configure_file(${CONFIG_H_FILE} ${BRLCAD_BINARY_DIR}/include/brlcad_config.h)


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
ADD_SUBDIRECTORY(src/rt)

OPTION(BRLCAD_PRINT_VERBOSE_SUMMARY "Print extra summary information" OFF)

if(BRLCAD_PRINT_VERBOSE_SUMMARY)
   MESSAGE("")
   MESSAGE("------ BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build (VERBOSE SUMMARY) ------")
   MESSAGE("")
else(BRLCAD_PRINT_VERBOSE_SUMMARY)
   MESSAGE("")
   MESSAGE("------ BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build  ------")
   MESSAGE("")
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


endif(BRLCAD_PRINT_VERBOSE_SUMMARY)
