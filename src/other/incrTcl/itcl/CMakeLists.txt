#                     C M A K E L I S T S . T X T
# ITCL
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
# ***                   ITCL CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(ITCL)

# Without Tcl libraries AND sources, we can't build Tk.  Use TCL_SRC_PREFIX
# and TCL_BIN_PREFIX to identify the location of the Tcl we are to use.
IF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX)
   SET(TCL_INCLUDE_DIRS ${TCL_SRC_PREFIX}/generic ${TCL_SRC_PREFIX}/libtommath)
   IF(WIN32)
      SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/win)
   ELSE(WIN32)
      SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/unix)
   ENDIF(WIN32)
   # Locate the TCL libraries.  We allow these to be overridden so that a parent
   # build can use a CMake target instead of an actual library path.
   IF(NOT TCL_LIBRARY)
      find_library(TCL_LIBRARY tcl PATHS ${TCL_BIN_PREFIX}/lib)
   ENDIF(NOT TCL_LIBRARY)
   IF(NOT TCL_STUB_LIBRARY)
      find_library(TCL_STUB_LIBRARY tclstub PATHS ${TCL_BIN_PREFIX}/lib)
   ENDIF(NOT TCL_STUB_LIBRARY)
   IF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY)
      MESSAGE(FATAL_ERROR "Could not find Tcl libraries in ${TCL_BIN_PREFIX} - please specify the parent directory for the libraries in the TCL_BIN_PREFIX variable (e.g. \${TCL_BIN_PREFIX}/lib)")
   ENDIF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY)
ELSE(TCL_SRC_PREFIX and TCL_BIN_PREFIX)
   MESSAGE(FATAL_ERROR "Please specify the source and library locations of Tcl using the TCL_SRC_PREFIX and TCL_BIN_PREFIX variables.")
ENDIF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(ITCL_MAJOR_VERSION 3)
SET(ITCL_MINOR_VERSION 4)

SET(ITCL_VERSION "${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}")

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


#-----------------------------------------------------------------------------
# Check if the compiler supports pipe - if so, use it
INCLUDE(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-pipe PIPE_COMPILER_FLAG)
IF(PIPE_COMPILER_FLAG)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
ENDIF(PIPE_COMPILER_FLAG)

#-----------------------------------------------------------------------------
# Set CMake module path
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

#-----------------------------------------------------------------------------
# Tcl/Tk's normal build system uses autotools macros, referred to as the
# TEA system.  An attempt to duplicate the required functionality from
# TEA is found in tcl.cmake
INCLUDE(${ITCL_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)
INCLUDE(${ITCL_SOURCE_DIR}/CMake/ac_std_funcs.cmake)
INCLUDE(${ITCL_SOURCE_DIR}/CMake/tcl.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
add_definitions(-DPACKAGE_NAME="itcl")
add_definitions(-DPACKAGE_TARNAME="itcl")
add_definitions(-DPACKAGE_VERSION="${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}")
add_definitions(-DPACKAGE_BUGREPORT="")

# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
ADD_TCL_CFLAG(STDC_HEADERS)

# Windows specific flags
IF(WIN32)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -MD)
	add_definitions(-Dinline=__inline -DBUILD_itcl)
ENDIF(WIN32)

INCLUDE(CheckLibraryExists)
INCLUDE(ResolveCompilerPaths)
IF(NOT WIN32)
   CHECK_LIBRARY_EXISTS(m cos "" HAVE_M_LIBRARY)
   IF(HAVE_M_LIBRARY)
      RESOLVE_LIBRARIES(M_LIBRARY "-lm")
   ENDIF(HAVE_M_LIBRARY)
ENDIF(NOT WIN32)


#----------------------------------------------------------------------------
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
#  add_definitions(-DNO_MEMMOVE=1) 
#  add_definitions(-DNO_STRING_H=1) 
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

int main () {return 0;}
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

SET(ITCL_WIN_SRCS
	win/dllEntryPoint.c
)

SET(ITCL_STUB_SRCS
	generic/itclStubLib.c
)


SET(ITCL_INCLUDE_DIRS ${ITCL_SOURCE_DIR}/generic ${ITCL_BINARY_DIR}/include)
MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)

include_directories(
	${TCL_INCLUDE_DIRS}
	${ITCL_INCLUDE_DIRS}
	${CMAKE_BINARY_DIR}/include
	${CMAKE_SOURCE_DIR}/include
)

IF(WIN32)
   add_library(itcl ${ITCL_GENERIC_SRCS} ${ITCL_WIN_SRCS})
ELSE(WIN32)
   add_library(itcl ${ITCL_GENERIC_SRCS})
ENDIF(WIN32)
target_link_libraries(itcl ${M_LIBRARY} ${TCL_STUB_LIBRARIES} )
SET_TARGET_PROPERTIES(itcl PROPERTIES PREFIX "lib")
SET_TARGET_PROPERTIES(itcl PROPERTIES VERSION ${ITCL_VERSION} SOVERSION ${ITCL_MAJOR_VERSION})
SET_PROPERTY(TARGET itcl APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
install(TARGETS itcl DESTINATION ${LIB_DIR})

add_library(itclstub STATIC ${ITCL_STUB_SRCS})
install(TARGETS itclstub DESTINATION ${LIB_DIR})
SET_PROPERTY(TARGET itclstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)

GET_TARGET_PROPERTY(ITCL_LIBLOCATION itcl LOCATION_${CMAKE_BUILD_TYPE})
GET_FILENAME_COMPONENT(ITCL_LIBNAME ${ITCL_LIBLOCATION} NAME)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded Itcl ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION} [list load [file join $dir .. .. ${LIB_DIR} ${ITCL_LIBNAME}] Itcl]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})

FILE(WRITE ${CMAKE_BINARY_DIR}/lib/itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}/pkgIndex.tcl "package ifneeded Itcl ${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION} [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${ITCL_LIBNAME}] Itcl]")

install(FILES library/itcl.tcl DESTINATION lib/itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/itcl.tcl ${CMAKE_BINARY_DIR}/lib/itcl${ITCL_MAJOR_VERSION}.${ITCL_MINOR_VERSION}/itcl.tcl COPYONLY)

SET(ITCL_HDRS
	generic/itcl.h
	generic/itclDecls.h
	generic/itclInt.h
	generic/itclIntDecls.h
	)
install(FILES ${ITCL_HDRS} DESTINATION include)
