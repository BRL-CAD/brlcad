#                     C M A K E L I S T S . T X T
# TCL
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

# Set CMake project name
PROJECT(TCL)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(TCL_VERSION_MAJOR 8)
SET(TCL_VERSION_MINOR 5)
SET(TCL_VERSION_PATCH 9)

SET(TCL_VERSION "${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}.${TCL_VERSION_PATCH}")

# For Windows, we need the Resource Compiler language
IF(WIN32)
	ENABLE_LANGUAGE(RC)
ENDIF(WIN32)

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


#----------------------------------------------------------------------------
# The location in which to install Tcl.  Only do this if CMAKE_INSTALL_PREFIX 
# hasn't been set already, to try and allow parent builds (if any) some control.
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   IF(NOT WIN32)
      IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			SET(CMAKE_INSTALL_PREFIX "/usr/local/tcldev-${TCL_VERSION}")
		ELSE ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
         SET(CMAKE_INSTALL_PREFIX "/usr")
		ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
   ENDIF(NOT WIN32)
   SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "TCL install prefix" FORCE)
   SET(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

#-----------------------------------------------------------------------------
# Output directories.

IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
   IF(WIN32)
      SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
   ELSE(WIN32)
      SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
   ENDIF(WIN32)
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
   SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
   SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

IF(0)
FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
   STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
   IF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
      IF(WIN32)
         SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
      ELSE(WIN32)
         SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
      ENDIF(WIN32)
   ENDIF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
   IF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
      SET("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
   ENDIF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
   IF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
      SET("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
   ENDIF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
ENDFOREACH()
ENDIF(0)

FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
	add_definitions(-DHAVE_ZLIB=1)
ENDIF(ZLIB_FOUND)
#-----------------------------------------------------------------------------
# If compiling on macosx, need to do something with tclConfig.h.in - figure
# out what




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
# Tcl/Tk's normal build system uses autotools macros, referred to as the
# TEA system.  An attempt to duplicate the required functionality from
# TEA is found in tcl.cmake
INCLUDE(${TCL_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)
INCLUDE(${TCL_SOURCE_DIR}/CMake/ac_std_funcs.cmake)
INCLUDE(${TCL_SOURCE_DIR}/CMake/tcl.cmake)

#----------------------------------------------------------------------------
# Define some standard configuration settings passed to all Tcl build targets
add_definitions(-DPACKAGE_NAME="tcl")
add_definitions(-DPACKAGE_TARNAME="tcl")
add_definitions(-DPACKAGE_VERSION="${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
add_definitions(-DPACKAGE_STRING="tcl ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
add_definitions(-DPACKAGE_BUGREPORT="")

# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
ADD_TCL_CFLAG(STDC_HEADERS)
ADD_TCL_CFLAG(CMAKE_HEADERS)

add_definitions(-DTCL_SHLIB_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}")

# Windows specific flags
IF(WIN32)
   add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -MD)
   add_definitions(-DTCL_PIPE_DLL="tclpip${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}.dll")
   add_definitions(-Dinline=__inline -DBUILD_tcl)
ENDIF(WIN32)

INCLUDE(CheckLibraryExists)
INCLUDE(ResolveCompilerPaths)
IF(NOT WIN32)
   CHECK_LIBRARY_EXISTS(m cos "" HAVE_M_LIBRARY)
   IF(HAVE_M_LIBRARY)
      RESOLVE_LIBRARIES(M_LIBRARY "-lm")
   ENDIF(HAVE_M_LIBRARY)
ENDIF(NOT WIN32)

IF(APPLE)
	ADD_TCL_CFLAG(MAC_OSX_TCL)
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		ADD_TCL_CFLAG(HAVE_COREFOUNDATION)
	ENDIF(COREFOUNDATION_FRAMEWORK)
ENDIF(APPLE)

#----------------------------------------------------------------------------
# Tom's math library - should MP_PREC be an option?
OPTION(TCL_TOMMATH "Enable Tom's Math Library" ON)
IF(TCL_TOMMATH)
	ADD_TCL_CFLAG(TCL_TOMMATH)
   add_definitions(-DMP_PREC=4)
ENDIF(TCL_TOMMATH)

#----------------------------------------------------------------------------

#This should probably be part of SC_TCL_64BIT_FLAGS()
IF(BRLCAD-ENABLE_64BIT)
	add_definitions(-DTCL_CFG_DO64BIT=1)
ENDIF(BRLCAD-ENABLE_64BIT)
IF(CMAKE_CL_64)
	add_definitions(-D_stati64=_stat64)
ENDIF(CMAKE_CL_64)


SC_MISSING_POSIX_HEADERS()

SC_ENABLE_THREADS()

SC_TCL_CFG_ENCODING()

SC_TCL_LINK_LIBS()

# TODO - look over SC_ENABLE_SHARED, SC_CONFIG_CFLAGS and SC_ENABLE_SYMBOLS
# need to decide how much of that logic is needed

SC_TCL_64BIT_FLAGS()

# Test endianness 
IF(NOT MSVC)
	INCLUDE(TestBigEndian)
	TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
	IF(WORDS_BIGENDIAN)
		ADD_TCL_CFLAG(WORDS_BIGENDIAN)
	ENDIF(WORDS_BIGENDIAN)
ENDIF(NOT MSVC)

# POSIX substitutes
CONFIG_CHECK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
IF(NOT HAVE_GETCWD)
	add_definitions(-DUSEGETWD=1) 
ENDIF(NOT HAVE_GETCWD)
IF(NOT HAVE_GETCWD)
	CONFIG_CHECK_FUNCTION_EXISTS(getwd HAVE_GETWD)
	IF(NOT HAVE_GETWD)
		ADD_TCL_CFLAG(NO_GETWD)
	ENDIF(NOT HAVE_GETWD)
ENDIF(NOT HAVE_GETCWD)
CONFIG_CHECK_FUNCTION_EXISTS(opendir HAVE_OPENDIR)
IF(NOT HAVE_OPENDIR)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/opendir.c)
	ADD_TCL_CFLAG(USE_DIRENT2_H)
ENDIF(NOT HAVE_OPENDIR)
CONFIG_CHECK_FUNCTION_EXISTS(strtol HAVE_STRTOL)
IF(NOT HAVE_STRTOL)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/strtol.c)
ENDIF(NOT HAVE_STRTOL)
CONFIG_CHECK_FUNCTION_EXISTS(waitpid HAVE_WAITPID)
IF(NOT HAVE_WAITPID)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/waitpid.c)
ENDIF(NOT HAVE_WAITPID)
CONFIG_CHECK_FUNCTION_EXISTS(strerror HAVE_STRERROR)
IF(NOT HAVE_STRERROR)
	ADD_TCL_CFLAG(NO_STRERROR)
ENDIF(NOT HAVE_STRERROR)
CHECK_FUNCTION_EXISTS(wait3 HAVE_WAIT3)
IF(NOT HAVE_WAIT3)
	ADD_TCL_CFLAG(NO_WAIT3)
ENDIF(NOT HAVE_WAIT3)
CONFIG_CHECK_FUNCTION_EXISTS(uname HAVE_UNAME)
IF(NOT HAVE_UNAME)
	ADD_TCL_CFLAG(NO_UNAME)
ENDIF(NOT HAVE_UNAME)
# Check for early Darwin version here - realpath
# is not threadsafe prior to Darwin 7
CHECK_FUNCTION_EXISTS(realpath HAVE_REALPATH)
IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND TCL_THREADS AND HAVE_REALPATH)
	STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
	IF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
		MESSAGE("realpath is not threadsafe in Darwin versions prior to 7, disabling")
		SET(HAVE_REALPATH)
	ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND TCL_THREADS AND HAVE_REALPATH)
IF(NOT HAVE_REALPATH)
	ADD_TCL_CFLAG(NO_REALPATH)
ENDIF(NOT HAVE_REALPATH)

SC_TCL_GETADDRINFO()

# Thread safe tests - need to go over macros and
# get the right logic going here
IF(TCL_THREADS)

	SC_TCL_GETPWUID_R()

	SC_TCL_GETPWNAM_R()

	SC_TCL_GETGRGID_R()

	SC_TCL_GETGRNAM_R()

	IF(NOT HAVE_GETHOSTBYNAME)
		CHECK_LIBRARY(GHBN lnsl gethostbyname)
		SET(HAVE_GETHOSTBYNAME ${GHBN})
	ENDIF(NOT HAVE_GETHOSTBYNAME)

	SC_TCL_GETHOSTBYADDR_R()
	SC_TCL_GETHOSTBYNAME_R()

	IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
		STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
		IF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
			ADD_TCL_CFLAG(HAVE_MTSAFE_GETHOSTBYNAME)
			ADD_TCL_CFLAG(HAVE_MTSAFE_GETHOSTBYADDR)
		ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
	ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

	IF(${CMAKE_SYSTEM_NAME} MATCHES "^HP-UX$")
		STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
		IF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 10)
			ADD_TCL_CFLAG(HAVE_MTSAFE_GETHOSTBYNAME)
			ADD_TCL_CFLAG(HAVE_MTSAFE_GETHOSTBYADDR)
		ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 10)
	ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^HP-UX$")

ENDIF(TCL_THREADS)

# Determine which interface to use to talk to the serial port.
SC_SERIAL_PORT()

# Do we need the sys/select.h check these days?


# Handle time
SC_TIME_HANDLER()

# memove check - need to revisit this, not finding memmove
# on OSX
#CHECK_FUNCTION_EXISTS(memmove, HAVE_MEMMOVE)
#IF(NOT HAVE_MEMMOVE)
#	add_definitions(-DNO_MEMMOVE=1) 
#	add_definitions(-DNO_STRING_H=1) 
#ENDIF(NOT HAVE_MEMMOVE)


# Test for bad functions
SC_TCL_CHECK_BROKEN_FUNC(
	strstr "
	extern int strstr();
	exit(strstr(\"\\\0test\", \"test\") ? 1 : 0);"
)


SC_TCL_CHECK_BROKEN_FUNC(
	strtoul "
	extern int strtoul();
	char *term, *string = \"0\";
	exit(strtoul(string,&term,0) != 0 || term != string+1);"
)

# Check for union wait

SET(UNION_WAIT_SRC "
#include <sys/types.h>
#include <sys/wait.h>
int
main ()
{

union wait x;
WIFEXITED(x);           /* Generates compiler error if WIFEXITED
                         * uses an int. */
return 0;
}
")
CHECK_C_SOURCE_RUNS("${UNION_WAIT_SRC}" HAVE_UNION_WAIT)
IF(NOT HAVE_UNION_WAIT)
	ADD_TCL_CFLAG(NO_UNION_WAIT)
ENDIF(NOT HAVE_UNION_WAIT)


# Check for types - TODO: still need to define substitutes if these
# are not found
CONFIG_CHECK_TYPE_SIZE(mode_t MODE)
CONFIG_CHECK_TYPE_SIZE(pid_t PID)
CONFIG_CHECK_TYPE_SIZE(size_t SIZE)
CONFIG_CHECK_TYPE_SIZE(uid_t UID)
CONFIG_CHECK_TYPE_SIZE(blkcnt_t BLKCNT)
CONFIG_CHECK_TYPE_SIZE(intptr_t INTPTR)
CONFIG_CHECK_TYPE_SIZE(uintptr_t UINTPTR)


SC_ENABLE_LANGINFO()

IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
	CONFIG_CHECK_FUNCTION_EXISTS(getattrlist HAVE_GETATTRLIST)
	CONFIG_CHECK_INCLUDE_FILE(copyfile.h HAVE_COPYFILE_H)
	CONFIG_CHECK_FUNCTION_EXISTS(copyfile HAVE_COPYFILE)
	IF(${COREFOUNDATION_FRAMEWORK})
		CONFIG_CHECK_INCLUDE_FILE(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
		CONFIG_CHECK_FUNCTION_EXISTS(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
		CONFIG_CHECK_FUNCTION_EXISTS(pthread_atfork HAVE_PTHREAD_ATFORK)
	ENDIF(${COREFOUNDATION_FRAMEWORK})
	ADD_TCL_CFLAG(USE_VFORK)
	add_definitions(-DTCL_DEFAULT_ENCODING="utf-8")
	ADD_TCL_CFLAG(TCL_LOAD_FROM_MEMORY)
	ADD_TCL_CFLAG(TCL_WIDE_CLICKS)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(AvailabilityMacros.h HAVE_AVAILABILITYMACROS_H)
	IF(HAVE_AVAILABILITYMACROS_H)
		SET(WEAK_IMPORT_SRCS "
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
#error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
#endif
#elif MAC_OS_X_VERSION_MIN_REQUIRED < 1020
#error MAC_OS_X_VERSION_MIN_REQUIRED < 1020
#endif
int rand(void) __attribute__((weak_import));
int main() {
rand();
return 0;
}
		")
		CHECK_C_SOURCE_COMPILES("${WEAK_IMPORT_SRCS}" WEAK_IMPORT_WORKING)
		IF(WEAK_IMPORT_WORKING)
			ADD_TCL_CFLAG(HAVE_WEAK_IMPORT)
		ENDIF(WEAK_IMPORT_WORKING)
		SET(SUSV3_SRCS "
#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
#error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
#endif
#elif MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#error MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#endif
#define _DARWIN_C_SOURCE 1
#include <sys/cdefs.h>

int main ()	{return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${SUSV3_SRCS}" SUSV3_WORKING)
		IF(SUSV3_WORKING)
			ADD_TCL_CFLAG(_DARWIN_C_SOURCE)
		ENDIF(SUSV3_WORKING)

	ENDIF(HAVE_AVAILABILITYMACROS_H)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

# Check for support of fts functions (readdir replacement)
SET(FTS_SRCS "
#include <sys/param.h>
#include <sys/stat.h>
#include <fts.h>

int main () {
char*const p[2] = {\"/\", NULL};
FTS *f = fts_open(p, FTS_PHYSICAL|FTS_NOCHDIR|FTS_NOSTAT, NULL);
FTSENT *e = fts_read(f); fts_close(f);
return 0;
}
")
CHECK_C_SOURCE_COMPILES("${FTS_SRCS}" FTS_WORKING)
IF(FTS_WORKING)
	ADD_TCL_CFLAG(HAVE_FTS)
ENDIF(FTS_WORKING)

OPTION(TCL_UNLOAD_DLLS "Allow unloading of shared libraries?" ON)
IF(TCL_UNLOAD_DLLS)
	ADD_TCL_CFLAG(TCL_UNLOAD_DLLS)
ENDIF(TCL_UNLOAD_DLLS)

# TODO - Need to set up check for timezone info

# TODO - Dtrace

#--------------------------------------------------------------------
#  Does the C stack grow upwards or downwards? Or cross-compiling?
#--------------------------------------------------------------------
SET(C_STACK_SRC "
int StackGrowsUp(int *parent) {
int here;
return (&here < parent);
}
int main (int argc, char *argv[]) {
int foo;
return StackGrowsUp(&foo);
}
")
CHECK_C_SOURCE_RUNS("${C_STACK_SRC}" STACK_GROWS_UP)
IF(STACK_GROWS_UP)
	ADD_TCL_CFLAG(TCL_STACK_GROWS_UP)
ENDIF(STACK_GROWS_UP)

CONFIG_CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CONFIG_CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CONFIG_CHECK_INCLUDE_FILE(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
   add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_SYS_FSTATFS_H)
CONFIG_CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CONFIG_CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CONFIG_CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CONFIG_CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CONFIG_CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

CHECK_COMPILER_SUPPORTS_HIDDEN_D()

CONFIG_CHECK_LIBRARY(DL dl dlopen)



CONFIG_CHECK_INCLUDE_FILE_USABILITY(sys/modem.h SYS_MODEM_H)


CHECK_FD_SET_IN_TYPES_D()

CONFIG_CHECK_FUNCTION_EXISTS(chflags HAVE_CHFLAGS)

IF(COMPAT_SRCS)
	ADD_TCL_CFLAG(USE_COMPAT)
ENDIF(COMPAT_SRCS)

IF(APPLE)
	# Now that all the tests are done, configure the tclConfig.h file:
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unix/tclConfig.h.in ${TCL_BINARY_DIR}/include/tclConfig.h)
ENDIF(APPLE)

ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(library)

SET(TCL_GENERIC_SRCS
	generic/regcomp.c
	generic/regexec.c
	generic/regfree.c
	generic/regerror.c
	generic/tclAlloc.c
	generic/tclAsync.c
	generic/tclBasic.c
	generic/tclBinary.c
	generic/tclCkalloc.c
	generic/tclClock.c
	generic/tclCmdAH.c
	generic/tclCmdIL.c
	generic/tclCmdMZ.c
	generic/tclCompCmds.c
	generic/tclCompExpr.c
	generic/tclCompile.c
	generic/tclConfig.c
	generic/tclDate.c
	generic/tclDictObj.c
	generic/tclEncoding.c
	generic/tclEnv.c
	generic/tclEvent.c
	generic/tclExecute.c
	generic/tclFCmd.c
	generic/tclFileName.c
	generic/tclGet.c
	generic/tclHash.c
	generic/tclHistory.c
	generic/tclIndexObj.c
	generic/tclInterp.c
	generic/tclIO.c
	generic/tclIOCmd.c
	generic/tclIOGT.c
	generic/tclIOSock.c
	generic/tclIOUtil.c
	generic/tclIORChan.c
	generic/tclLink.c
	generic/tclListObj.c
	generic/tclLiteral.c
	generic/tclLoad.c
	generic/tclMain.c
	generic/tclNamesp.c
	generic/tclNotify.c
	generic/tclObj.c
	generic/tclPanic.c
	generic/tclParse.c
	generic/tclPathObj.c
	generic/tclPipe.c
	generic/tclPkg.c
	generic/tclPkgConfig.c
	generic/tclPosixStr.c
	generic/tclPreserve.c
	generic/tclProc.c
	generic/tclRegexp.c
	generic/tclResolve.c
	generic/tclResult.c
	generic/tclScan.c
	generic/tclStubInit.c
	generic/tclStringObj.c
	generic/tclStrToD.c
	generic/tclTest.c
	generic/tclTestObj.c
	generic/tclTestProcBodyObj.c
	generic/tclThread.c
	generic/tclThreadAlloc.c
	generic/tclThreadJoin.c
	generic/tclThreadStorage.c
	generic/tclTimer.c
	generic/tclTrace.c
	generic/tclUtf.c
	generic/tclUtil.c
	generic/tclVar.c
	generic/tclTomMathInterface.c
	generic/tclUniData.c
)

SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man")

SET(TCL_STUB_SRCS
	generic/tclStubLib.c
)

SET(TCL_TOMMATH_SRCS
	libtommath/bncore.c
	libtommath/bn_reverse.c
	libtommath/bn_fast_s_mp_mul_digs.c
	libtommath/bn_fast_s_mp_sqr.c
	libtommath/bn_mp_add.c
	libtommath/bn_mp_add_d.c
	libtommath/bn_mp_and.c
	libtommath/bn_mp_clamp.c
	libtommath/bn_mp_clear.c
	libtommath/bn_mp_clear_multi.c
	libtommath/bn_mp_cmp.c
	libtommath/bn_mp_cmp_d.c
	libtommath/bn_mp_cmp_mag.c
	libtommath/bn_mp_copy.c
	libtommath/bn_mp_count_bits.c
	libtommath/bn_mp_div.c
	libtommath/bn_mp_div_d.c
	libtommath/bn_mp_div_2.c
	libtommath/bn_mp_div_2d.c
	libtommath/bn_mp_div_3.c
	libtommath/bn_mp_exch.c
	libtommath/bn_mp_expt_d.c
	libtommath/bn_mp_grow.c
	libtommath/bn_mp_init.c
	libtommath/bn_mp_init_copy.c
	libtommath/bn_mp_init_multi.c
	libtommath/bn_mp_init_set.c
	libtommath/bn_mp_init_size.c
	libtommath/bn_mp_karatsuba_mul.c
	libtommath/bn_mp_karatsuba_sqr.c
	libtommath/bn_mp_lshd.c
	libtommath/bn_mp_mod.c
	libtommath/bn_mp_mod_2d.c
	libtommath/bn_mp_mul.c
	libtommath/bn_mp_mul_2.c
	libtommath/bn_mp_mul_2d.c
	libtommath/bn_mp_mul_d.c
	libtommath/bn_mp_neg.c
	libtommath/bn_mp_or.c
	libtommath/bn_mp_radix_size.c
	libtommath/bn_mp_radix_smap.c
	libtommath/bn_mp_read_radix.c
	libtommath/bn_mp_rshd.c
	libtommath/bn_mp_set.c
	libtommath/bn_mp_shrink.c
	libtommath/bn_mp_sqr.c
	libtommath/bn_mp_sqrt.c
	libtommath/bn_mp_sub.c
	libtommath/bn_mp_sub_d.c
	libtommath/bn_mp_to_unsigned_bin.c
	libtommath/bn_mp_to_unsigned_bin_n.c
	libtommath/bn_mp_toom_mul.c
	libtommath/bn_mp_toom_sqr.c
	libtommath/bn_mp_toradix_n.c
	libtommath/bn_mp_unsigned_bin_size.c
	libtommath/bn_mp_xor.c
	libtommath/bn_mp_zero.c
	libtommath/bn_s_mp_add.c
	libtommath/bn_s_mp_mul_digs.c
	libtommath/bn_s_mp_sqr.c
	libtommath/bn_s_mp_sub.c
)

SET(TCL_OSX_SRCS
	macosx/tclMacOSXBundle.c
	macosx/tclMacOSXFCmd.c
	macosx/tclMacOSXNotify.c
)

SET(TCL_WIN_SRCS
	win/tcl.rc
	win/tclAppInit.c
	win/tclWin32Dll.c
	win/tclWinChan.c
	win/tclWinConsole.c
	win/tclWinDde.c
	win/tclWinError.c
	win/tclWinFCmd.c
	win/tclWinFile.c
	win/tclWinInit.c
	win/tclWinLoad.c
	win/tclWinNotify.c
	win/tclWinPipe.c
	win/tclWinReg.c
	win/tclWinSerial.c
	win/tclWinSock.c
	win/tclWinThrd.c
	win/tclWinTime.c
)

SET_PROPERTY(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
SET_PROPERTY(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")

SET(TCL_UNIX_SRCS
	unix/tclAppInit.c
	unix/tclUnixChan.c
	unix/tclUnixEvent.c
	unix/tclUnixFCmd.c
	unix/tclUnixFile.c
	unix/tclUnixPipe.c
	unix/tclUnixSock.c
	unix/tclUnixTest.c
	unix/tclUnixThrd.c
	unix/tclUnixTime.c
	unix/tclUnixInit.c
	unix/tclUnixCompat.c
)

SET_PROPERTY(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
SET_PROPERTY(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")

SET(TCL_NOTIFY_SRCS
	unix/tclUnixNotfy.c
)

SET(TCL_DL_SRCS
	unix/tclLoadAix.c
	unix/tclLoadDl.c
	unix/tclLoadDyld.c
   generic/tclLoadNone.c
  	unix/tclLoadOSF.c
	unix/tclLoadShl.c
)


SET(TCL_SRCS ${TCL_GENERIC_SRCS} ${TCL_TOMMATH_SRCS} ${TCL_STUB_SRCS}) 
IF(WIN32)
	SET(TCL_SRCS ${TCL_SRCS} ${TCL_WIN_SRCS})
ELSE(WIN32)
	SET(TCL_SRCS ${TCL_SRCS} ${TCL_NOTIFY_SRCS} ${TCL_UNIX_SRCS})
	IF(APPLE)
		SET(TCL_SRCS ${TCL_SRCS} ${TCL_OSX_SRCS} unix/tclLoadDyld.c)
	ENDIF(APPLE)
	IF(NOT APPLE)
		SET(TCL_SRCS ${TCL_SRCS} unix/tclLoadDl.c)
	ENDIF(NOT APPLE)
ENDIF(WIN32)

SET(TCL_INCLUDE_DIRS ${TCL_SOURCE_DIR}/generic ${TCL_SOURCE_DIR}/libtommath ${TCL_BINARY_DIR}/include)
IF(WIN32)
	SET(TCL_INCLUDE_DIRS ${TCL_SOURCE_DIR}/win ${TCL_INCLUDE_DIRS})
ELSE(WIN32)
	SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SOURCE_DIR}/unix)
ENDIF(WIN32)

include_directories(
	${TCL_INCLUDE_DIRS}
)

IF(WIN32)
	SET(TCLSH_SRCS win/tclAppInit.c win/tclsh.rc)
ELSE(WIN32)
	SET(TCLSH_SRCS unix/tclAppInit.c)
ENDIF(WIN32)

IF(NOT LIB_DIR)
	SET(LIB_DIR lib)
ENDIF(NOT LIB_DIR)
add_library(tcl ${TCL_SRCS})
install(TARGETS tcl DESTINATION ${LIB_DIR})
target_link_libraries(tcl ${COREFOUNDATION_FRAMEWORK} ${SUN_MATH_LIB} ${TCL_LINK_LIBS} ${M_LIBRARY} ${DL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
GET_TARGET_PROPERTY(TCL_INSTALL_LIBNAME tcl LOCATION)
SET_TARGET_PROPERTIES(tcl PROPERTIES VERSION ${TCL_VERSION} SOVERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})
IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
	SET_TARGET_PROPERTIES(tcl PROPERTIES LINK_FLAGS "-compatibility_version ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR} -current_version ${TCL_VERSION} -install_name ${TCL_INSTALL_LIBNAME} -seg1addr 0xa000000 -sectcreate __TEXT __info_plist ${TCL_BINARY_DIR}/Tcl-Info.plist")
	SET(EXTRA_TCLSH_LIBS "-sectcreate __TEXT __info_plist ${TCL_BINARY_DIR}/Tclsh-Info.plist")
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tcl-Info.plist.in ${TCL_BINARY_DIR}/Tcl-Info.plist)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tclsh-Info.plist.in ${TCL_BINARY_DIR}/Tclsh-Info.plist)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")


add_library(tclstub STATIC ${TCL_STUB_SRCS})
target_link_libraries(tclstub ${CMAKE_THREAD_LIBS_INIT} ${COREFOUNDATION_FRAMEWORK} ${M_LIBRARY} ${DL_LIBRARY})
set_target_properties(tclstub PROPERTIES COMPILE_FLAGS "-fPIC")
install(TARGETS tclstub DESTINATION ${LIB_DIR})

add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl ${SUN_MATH_LIB} ${M_LIBRARY} ${EXTRA_TCLSH_LIBS})
install(TARGETS tclsh DESTINATION bin)
SET_TARGET_PROPERTIES(tclsh PROPERTIES VERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})

SET(TCL_HDRS
	generic/tcl.h
	generic/tclDecls.h
	generic/tclPlatDecls.h
	generic/tclTomMath.h
	generic/tclTomMathDecls.h
	)
install(FILES ${TCL_HDRS} DESTINATION include)

MARK_AS_ADVANCED(TCL_LIBRARIES TCL_CONF_PREFIX TCL_INCLUDE_DIRS TCL_INCLUDE_PATH)
MARK_AS_ADVANCED(TCL_STUB_LIBRARIES TCL_TCLSH TCL_TCLSH_EXECUTABLE)
MARK_AS_ADVANCED(TCL_THREADS TCL_TK_CONF_PREFIX TCL_TOMMATH TCL_UNLOAD_DLLS)
MARK_AS_ADVANCED(TCL_VERSION_MAJOR TCL_VERSION_MINOR TCL_WISH_EXECUTABLE)

