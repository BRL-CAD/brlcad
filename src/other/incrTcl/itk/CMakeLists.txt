#                     C M A K E L I S T S . T X T
# ITK 
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
PROJECT(ITK)

# set local CFLAGS name
SET(CFLAGS_NAME ITK)
SET(ITK_CFLAGS "")
MARK_AS_ADVANCED(ITK_CFLAGS)
SET(CONFIG_CFLAGS ITK_CFLAGS)

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(ITK_MAJOR_VERSION 3)
SET(ITK_MINOR_VERSION 4)

SET(ITK_VERSION "${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION}")

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
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

#-----------------------------------------------------------------------------
# Tcl/Tk's normal build system uses autotools macros, referred to as the
# TEA system.  An attempt to duplicate the required functionality from
# TEA is found in tcl.cmake
INCLUDE(${ITK_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)
INCLUDE(${ITK_SOURCE_DIR}/CMake/tcl.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
SET(ITK_CFLAGS "${ITK_CFLAGS} -DPACKAGE_NAME=\\\"itk\\\"")
SET(ITK_CFLAGS "${ITK_CFLAGS} -DPACKAGE_TARNAME=\\\"itk\\\"")
SET(ITK_CFLAGS "${ITK_CFLAGS} -DPACKAGE_VERSION=\\\"${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION}\\\"")
SET(ITK_CFLAGS "${ITK_CFLAGS} -DPACKAGE_BUGREPORT=\\\"\\\"")
SET(ITK_CFLAGS "${ITK_CFLAGS} -DSTDC_HEADERS=1")

#----------------------------------------------------------------------------

IF(WIN32)
	SET(ITK_CFLAGS "${ITK_CFLAGS} -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -MD")
	SET(ITK_CFLAGS "${ITK_CFLAGS} -Dinline=__inline -DBUILD_itk")
ENDIF(WIN32)

SET(TCL_CFLAGS "")

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
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DWORDS_BIGENDIAN=1")
	ENDIF(WORDS_BIGENDIAN)
ENDIF(NOT MSVC)

# POSIX substitutes
CONFIG_CHECK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
IF(NOT HAVE_GETCWD)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DUSEGETWD=1") 
ENDIF(NOT HAVE_GETCWD)
IF(NOT HAVE_GETCWD)
	CONFIG_CHECK_FUNCTION_EXISTS(getwd HAVE_GETWD)
	IF(NOT HAVE_GETWD)
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_GETWD=1") 
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
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_STRERROR=1") 
ENDIF(NOT HAVE_STRERROR)
CHECK_FUNCTION_EXISTS(wait3 HAVE_WAIT3)
IF(NOT HAVE_WAIT3)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_WAIT3=1") 
ENDIF(NOT HAVE_WAIT3)
CONFIG_CHECK_FUNCTION_EXISTS(uname HAVE_UNAME)
IF(NOT HAVE_UNAME)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_UNAME=1") 
ENDIF(NOT HAVE_UNAME)
# Should be checking for early Darwin version here - realpath
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
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_REALPATH=1") 
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
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_MTSAFE_GETHOSTBYNAME=1")
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_MTSAFE_GETHOSTBYADDR=1")
		ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
	ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

	IF(${CMAKE_SYSTEM_NAME} MATCHES "^HP-UX$")
		STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
		IF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 10)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_MTSAFE_GETHOSTBYNAME=1")
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_MTSAFE_GETHOSTBYADDR=1")
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
#	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_MEMMOVE=1") 
#	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_STRING_H=1") 
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
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_DEFAULT_ENCODING=\\\"utf-8\\\"")
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
   SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_FSTATFS=1")
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

find_package(X11)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ITK_CFLAGS} ${TCL_CFLAGS}")



SET(ITK_GENERIC_SRCS
	generic/itkStubInit.c
	generic/itkStubLib.c
	generic/itk_archetype.c
	generic/itk_cmds.c
	generic/itk_option.c
	generic/itk_util.c
)

SET(ITK_WIN_SRCS
	win/dllEntryPoint.c
)

SET(ITK_STUB_SRCS
	generic/itkStubLib.c
)

# When it comes to identification of the location of the Tcl
# sources, the convention is as follows:
#
# 1.  If TCL_INCLUDE_DIRS is defined, use it
# 2.  Else, if TCL_PREFIX is defined build include paths from it
# 3.  If TCL_INCLUDE_DIRS is not defined and TCL_BIN_PREFIX is,
#     add include directories based on the presumption of the
#     binary build dir (and hence tclConfig.h) being in a non-src
#     location.
#
# It's still possible, depending on how Tcl was built, to 
# have a situation where TCL_INCLUDE_DIRS has to be augmented
# manually.  However, the logic below should cover the most common cases.

IF(NOT TCL_INCLUDE_DIRS)
	IF(TCL_PREFIX)
		SET(TCL_INCLUDE_DIRS ${TCL_PREFIX}/generic ${TCL_PREFIX}/libtommath)
		IF(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_PREFIX}/win)
		ELSE(WIN32)
			SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_PREFIX}/unix)
		ENDIF(WIN32)
	ENDIF(TCL_PREFIX)
	IF(TCL_BIN_PREFIX)
		SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_BIN_PREFIX}/include)
	ENDIF(TCL_BIN_PREFIX)
ENDIF(NOT TCL_INCLUDE_DIRS)

IF(NOT ITCL_INCLUDE_DIRS)
	IF(ITCL_PREFIX)
		SET(ITCL_INCLUDE_DIRS ${ITCL_PREFIX}/generic ${ITCL_PREFIX}/libtommath)
		IF(WIN32)
			SET(ITCL_INCLUDE_DIRS ${ITCL_INCLUDE_DIRS} ${ITCL_PREFIX}/win)
		ELSE(WIN32)
			SET(ITCL_INCLUDE_DIRS ${ITCL_INCLUDE_DIRS} ${ITCL_PREFIX}/unix)
		ENDIF(WIN32)
	ENDIF(ITCL_PREFIX)
	IF(ITCL_BIN_PREFIX)
		SET(ITCL_INCLUDE_DIRS ${ITCL_INCLUDE_DIRS} ${ITCL_BIN_PREFIX}/include)
	ENDIF(ITCL_BIN_PREFIX)
ENDIF(NOT ITCL_INCLUDE_DIRS)

IF(NOT TK_INCLUDE_PATH)
	IF(TK_PREFIX)
		SET(TK_INCLUDE_PATH ${TK_PREFIX}/generic ${TK_PREFIX}/libtommath)
		IF(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_PREFIX}/win)
		ELSE(WIN32)
			SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_PREFIX}/unix)
		ENDIF(WIN32)
	ENDIF(TK_PREFIX)
	IF(TK_BIN_PREFIX)
		SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_BIN_PREFIX}/include)
	ENDIF(TK_BIN_PREFIX)
ENDIF(NOT TK_INCLUDE_PATH)


SET(ITK_INCLUDE_PATH ${ITK_SOURCE_DIR}/generic ${ITK_BINARY_DIR}/include)

include_directories(
	${TCL_INCLUDE_DIRS}
	${TK_INCLUDE_PATH}
	${ITCL_INCLUDE_DIRS}
	${ITK_INCLUDE_PATH}
	${X11_INCLUDE_DIR}
	${CMAKE_BINARY_DIR}/include
	${CMAKE_SOURCE_DIR}/include
)

IF(WIN32)
	add_library(itk ${ITK_GENERIC_SRCS} ${ITK_WIN_SRCS})
ELSE(WIN32)
	add_library(itk ${ITK_GENERIC_SRCS})
ENDIF(WIN32)
target_link_libraries(itk ${M_LIBRARY} ${TCL_LIBRARIES} ${ITCL_LIBRARY})
SET_TARGET_PROPERTIES(itk PROPERTIES VERSION ${ITK_VERSION} SOVERSION ${ITK_MAJOR_VERSION})
SET_TARGET_PROPERTIES(itk PROPERTIES PREFIX "lib")
install(TARGETS itk DESTINATION ${LIB_DIR})

add_library(itkstub STATIC ${ITK_STUB_SRCS})
target_link_libraries(itkstub ${ITCL_STUB_LIBRARY} ${TCL_STUB_LIBRARIES})
install(TARGETS itkstub DESTINATION ${LIB_DIR})

GET_TARGET_PROPERTY(ITK_LIBLOCATION itk LOCATION_${CMAKE_BUILD_TYPE})
GET_FILENAME_COMPONENT(ITK_LIBNAME ${ITK_LIBLOCATION} NAME)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded Itk ${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION} [list load [file join $dir .. .. ${LIB_DIR} ${ITK_LIBNAME}] Itk]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

FILE(WRITE ${CMAKE_BINARY_DIR}/lib/itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION}/pkgIndex.tcl "package ifneeded Itk ${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION} [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${ITK_LIBNAME}] Itk]")


SET(ITK_LIBRARY_FILES
	library/Archetype.itk
	library/Toplevel.itk
	library/Widget.itk
	library/itk.tcl
	library/tclIndex
)
install(FILES ${ITK_LIBRARY_FILES} DESTINATION lib/itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION})

FOREACH(file ${ITK_LIBRARY_FILES})
   get_filename_component(filename ${file} NAME)
	configure_file(${file} ${CMAKE_BINARY_DIR}/lib/itk${ITK_MAJOR_VERSION}.${ITK_MINOR_VERSION}/${filename} COPYONLY)
ENDFOREACH(file ${ITK_LIBRARY_FILES})

SET(ITK_HDRS
	generic/itk.h
	generic/itkDecls.h
	)
install(FILES ${ITK_HDRS} DESTINATION include)
