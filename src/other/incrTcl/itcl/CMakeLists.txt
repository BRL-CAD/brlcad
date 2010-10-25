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
# ***                    Tcl CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(ITCL)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(ITCL_MAJOR_VERSION 8)
SET(ITCL_MINOR_VERSION 5)
SET(ITCL_PATCH_LEVEL 9)

SET(ITCL_VERSION "${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}.${ITCL_PATCH_LEVEL}")

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
	IF(WIN32)
		SET(CMAKE_INSTALL_PREFIX "/usr/local")
	ELSE(WIN32) 
		SET(CMAKE_INSTALL_PREFIX "C:/Tcl")
	ENDIF(WIN32) 
ENDIF(NOT CMAKE_INSTALL_PREFIX)


#-----------------------------------------------------------------------------
# Check if the compiler supports pipe - if so, use it
INCLUDE(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-pipe PIPE_COMPILER_FLAG)
IF(PIPE_COMPILER_FLAG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
ENDIF(PIPE_COMPILER_FLAG)

#-----------------------------------------------------------------------------
# Set CMake module path
SET(CMAKE_MODULE_PATH "${TCL_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

#-----------------------------------------------------------------------------
# Unlike the autotools based builds, CMake will always use itclConfig.h

add_definitions(
	-DHAVE_ITCL_CONFIG_H
)

# We will need a tclConfig.h.in file to hold all the #cmakedefine statements,
# which will in turn be used to generate a tclConfig.h file.  To succeed, the 
# file must first be present, which we take care of here:

SET(CONFIG_H_FILE "${CMAKE_BINARY_DIR}/include/itclConfig.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** itcl define statements from CMake ****/\n")
FILE(APPEND ${CONFIG_H_FILE} "#ifndef _ITCLCONFIG\n")
FILE(APPEND ${CONFIG_H_FILE} "#define _ITCLCONFIG\n")

INCLUDE(${ITCL_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"Itcl\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"Itcl\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_VERSION \"${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"Itcl ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#define ITCL_LIBRARY \"${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}itcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define USE_TCL_STUBS 1\n")

#----------------------------------------------------------------------------
# Define a number of variables to be controlled by options
OPTION(TCL_THREADS "Enable Tcl Thread support" ON)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_THREADS 1\n")
IF(TCL_THREADS)
	FILE(APPEND ${CONFIG_H_FILE} "#define USE_THREAD_ALLOC 1\n")
	FILE(APPEND ${CONFIG_H_FILE} "#define _REENTRANT 1\n")
	FILE(APPEND ${CONFIG_H_FILE} "#define _THREAD_SAFE 1\n")
	IF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
		FILE(APPEND ${CONFIG_H_FILE} "#define _POSIX_PTHREAD_SEMANTICS 1\n")
	ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
ENDIF(TCL_THREADS)

#----------------------------------------------------------------------------
# Run tests for system headers and functionality
CHECK_INCLUDE_FILE_H(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE_H(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE_H(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
   FILE(APPEND ${CONFIG_H_FILE} "#define NO_FSTATFS 1\n")
ENDIF(NOT HAVE_SYS_FSTATFS_H)
CHECK_INCLUDE_FILE_H(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE_H(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE_H(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE_H(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE_H(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE_H(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE_H(unistd.h HAVE_UNISTD_H)

CHECK_INCLUDE_FILE_H(dirent.h HAVE_DIRENT_H)
IF(NOT HAVE_DIRENT_H)
	FILE(APPEND ${CONFIG_H_FILE} "#define NO_DIRENT_H 1\n")
ENDIF(NOT HAVE_DIRENT_H)

CHECK_INCLUDE_FILE_USABILITY_H(float.h FLOAT_H)
CHECK_INCLUDE_FILE_USABILITY_H(values.h VALUES_H)
CHECK_INCLUDE_FILE_USABILITY_H(limits.h LIMITS_H)
CHECK_INCLUDE_FILE_USABILITY_H(sys/wait.h SYS_WAIT_H)
CHECK_INCLUDE_FILE_USABILITY_H(dlfcn.h DLFCN_H)
CHECK_INCLUDE_FILE_USABILITY_H(sys/param.h SYS_PARAM_H)


CHECK_LIBRARY(M m sin)
CHECK_LIBRARY(IEEE ieee main)
CHECK_LIBRARY(INET inet main)

CHECK_INCLUDE_FILE_USABILITY_H(net/errno.h NET_ERRNO_H)

CHECK_FUNCTION_EXISTS(connect HAVE_CONNECT)
CHECK_FUNCTION_EXISTS(gethostbyname HAVE_GETHOSTBYNAME)
IF(NOT HAVE_GETHOSTBYNAME)
	CHECK_LIBRARY(GHBN lnsl gethostbyname)
	SET(HAVE_GETHOSTBYNAME ${GHBN})
ENDIF(NOT HAVE_GETHOSTBYNAME)

CHECK_COMPILER_SUPPORTS_HIDDEN()

CHECK_LIBRARY(DL dl dlopen)

CHECK_FUNCTION_EXISTS_H(getcwd HAVE_GETCWD)
CHECK_FUNCTION_EXISTS_H(opendir HAVE_OPENDIR)
CHECK_FUNCTION_EXISTS_H(strtol HAVE_STRTOL)
CHECK_FUNCTION_EXISTS_H(waitpid HAVE_WAITPID)
CHECK_FUNCTION_EXISTS_H(strerror HAVE_STRERROR)
CHECK_FUNCTION_EXISTS_H(getwd HAVE_GETWD)
CHECK_FUNCTION_EXISTS_H(wait3 HAVE_WAIT3)
CHECK_FUNCTION_EXISTS_H(uname HAVE_UNAME)
CHECK_FUNCTION_EXISTS_H(realpath HAVE_REALPATH)
CHECK_FUNCTION_EXISTS(getaddrinfo HAVE_GETADDRINFO)
IF(HAVE_GETADDRINFO)
	CHECK_GETADDERINFO_WORKING()
ENDIF(HAVE_GETADDRINFO)


CHECK_INCLUDE_FILE_USABILITY_H(sys/modem.h SYS_MODEM_H)

TERMIOS_TERMIO_SGTTY()

CHECK_FD_SET_IN_TYPES_H()

CHECK_INCLUDE_FILE_USABILITY_H(sys/time.h HAVE_SYS_TIME_H)
CHECK_TIME_AND_SYS_TIME()

CHECK_FUNCTION_EXISTS_H(gmtime_r HAVE_GMTIME_R)
CHECK_FUNCTION_EXISTS_H(localtime_r HAVE_LOCALTIME_R)
CHECK_FUNCTION_EXISTS_H(mktime HAVE_MKTIME)

CHECK_STRUCT_HAS_MEMBER_H("struct tm" tm_tzadj time.h HAVE_TM_TZADJ)
CHECK_STRUCT_HAS_MEMBER_H("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)

CHECK_INCLUDE_FILE_H(copyfile.h HAVE_COPYFILE_H)
CHECK_FUNCTION_EXISTS_H(copyfile HAVE_COPYFILE)

CHECK_INCLUDE_FILE_H(sys/ioctl.h HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILE_H(sys/filio.h HAVE_SYS_FILIO_H)



#CHECK_FUNCTION_EXISTS_H( HAVE_GETPWUID_R_5)
CHECK_FUNCTION_EXISTS_H(getpwuid HAVE_GETPWUID_R)
#CHECK_FUNCTION_EXISTS_H( HAVE_GETPWNAM_R_5)
CHECK_FUNCTION_EXISTS_H(getpwnam HAVE_GETPWNAM_R)
#CHECK_FUNCTION_EXISTS_H( HAVE_GETGRGID_R_5)
CHECK_FUNCTION_EXISTS_H(getgrgid HAVE_GETGRGID_R)
#CHECK_FUNCTION_EXISTS_H( HAVE_GETGRNAM_R_5)
CHECK_FUNCTION_EXISTS_H(getgrnam HAVE_GETGRNAM_R)
#CHECK_FUNCTION_EXISTS_H( HAVE_MTSAFE_GETHOSTBYNAME)
#CHECK_FUNCTION_EXISTS_H( HAVE_MTSAFE_GETHOSTBYADDR)
#CHECK_FUNCTION_EXISTS_H( HAVE_STRUCT_STAT_ST_BLOCKS)
#CHECK_FUNCTION_EXISTS_H( HAVE_STRUCT_STAT_ST_BLKSIZE)
CHECK_TYPE_SIZE_H(blkcnt_t HAVE_BLKCNT_T)
CHECK_TYPE_SIZE_H(intptr_t HAVE_INTPTR_T)
CHECK_TYPE_SIZE_H(uintptr_t HAVE_UINTPTR_T)
#CHECK_TYPE_SIZE_H( HAVE_SIGNED_CHAR)
#CHECK_TYPE_SIZE_H( HAVE_LANGINFO)
#CHECK_TYPE_SIZE_H( HAVE_CHFLAGS)
CHECK_FUNCTION_EXISTS_H(getattrlist HAVE_GETATTRLIST)
CHECK_INCLUDE_FILE_H(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
CHECK_FUNCTION_EXISTS_H(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
CHECK_FUNCTION_EXISTS_H(pthread_atfork HAVE_PTHREAD_ATFORK)
#CHECK_INCLUDE_FILE_H( USE_VFORK)
#CHECK_INCLUDE_FILE_H( TCL_WIDE_INT_TYPE long\ long\n")
#CHECK_INCLUDE_FILE_H( TCL_CFG_DEBUG)
#CHECK_INCLUDE_FILE_H( USE_TERMIOS)
#CHECK_INCLUDE_FILE_H( TIME_WITH_SYS_TIME)
#CHECK_INCLUDE_FILE_H( HAVE_TM_GMTOFF)
#CHECK_INCLUDE_FILE_H( HAVE_TIMEZONE_VAR)
#CHECK_INCLUDE_FILE_H( HAVE_STRUCT_STAT64)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine M_PREC 4\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_DEFAULT_ENCODING \"utf-8\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_LOAD_FROM_MEMORY 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_WIDE_CLICKS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_AVAILABILITYMACROS_H 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_WEAK_IMPORT 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine _DARWIN_C_SOURCE 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_FTS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_UNLOAD_DLLS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine USE_DTRACE 1\n")

FIND_PACKAGE(Threads)

# Now that all the tests are done, configure the itclConfig.h file:
FILE(APPEND ${CONFIG_H_FILE} "#endif /*_ITCLCONFIG*/\n")
configure_file(${CONFIG_H_FILE} ${ITCL_BINARY_DIR}/include/itclConfig.h)

SET(ITCL_GENERIC_SRCS
	generic/itclStubInit.c
	generic/itclStubLib.c
	generic/itcl_bicmds.c
	generic/itcl_class.c
	generic/itcl_cmds.c
	generic/itcl_ensemble.c
	generic/itcl_linkage.c
	generic/itcl_methods.c
	generic/itcl_migrate.c
	generic/itcl_objects.c
	generic/itcl_parse.c
	generic/itcl_util.c
)

SET(ITCL_STUB_SRCS
	generic/itclStubLib.c
)

# When it comes to identification of the location of the Tcl
# sources, the convention is as follows:
#
# 1.  If TCL_INCLUDE_PATH is defined, use it
# 2.  Else, if TCL_PREFIX is defined build include paths from it
# 3.  If TCL_INCLUDE_PATH is not defined and TCL_BIN_PREFIX is,
#     add include directories based on the presumption of the
#     binary build dir (and hence tclConfig.h) being in a non-src
#     location.
#
# It's still possible, depending on how Tcl was built, to 
# have a situation where TCL_INCLUDE_PATH has to be augmented
# manually.  However, the logic below should cover the most common cases.

IF(NOT TCL_INCLUDE_PATH)
	IF(TCL_PREFIX)
		SET(TCL_INCLUDE_PATH ${TCL_PREFIX}/generic ${TCL_PREFIX}/libtommath)
		IF(WIN32)
			SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_PREFIX}/win)
		ELSE(WIN32)
			SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_PREFIX}/unix)
		ENDIF(WIN32)
	ENDIF(TCL_PREFIX)
	IF(TCL_BIN_PREFIX)
		SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_BIN_PREFIX}/include)
	ENDIF(TCL_BIN_PREFIX)
ENDIF(NOT TCL_INCLUDE_PATH)


SET(ITCL_INCLUDE_PATH ${ITCL_SOURCE_DIR}/generic ${ITCL_BINARY_DIR}/include)

include_directories(
	${TCL_INCLUDE_PATH}
	${ITCL_INCLUDE_PATH}
)

add_library(itcl ${ITCL_GENERIC_SRCS})
target_link_libraries(itcl ${M_LIBRARY} ${TCL_LIBRARY})
install(TARGETS itcl DESTINATION lib/Itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded Itcl ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION} [list load [file join $dir libitcl${CMAKE_SHARED_LIBRARY_SUFFIX}] Itcl]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/Itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
