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
PROJECT(TCL)

# set local CFLAGS name
SET(CFLAGS_NAME TCL)
SET(TCL_CFLAGS "")

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(TCL_MAJOR_VERSION 8)
SET(TCL_MINOR_VERSION 5)
SET(TCL_PATCH_LEVEL 8)

SET(TCL_VERSION "${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}.${TCL_PATCH_LEVEL}")

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
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_NAME=\"tcl\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_TARNAME=\"tcl\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_VERSION=\"${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_STRING=\"tcl\\ ${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DPACKAGE_BUGREPORT=\"\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DSTDC_HEADERS=1")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_SHLIB_EXT=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_INSTALL_LIBDIR=\"${CMAKE_INSTALL_PREFIX}/lib\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_INSTALL_BINDIR=\"${CMAKE_INSTALL_PREFIX}/bin\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_INSTALL_SCRDIR=\"${CMAKE_INSTALL_PREFIX}/scripts\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_INSTALL_INCDIR=\"${CMAKE_INSTALL_PREFIX}/include\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_INSTALL_DOCDIR=\"${CMAKE_INSTALL_PREFIX}/share/man\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_RUNTIME_LIBDIR=\"${CMAKE_INSTALL_PREFIX}/lib\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_RUNTIME_BINDIR=\"${CMAKE_INSTALL_PREFIX}/bin\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_RUNTIME_SCRDIR=\"${CMAKE_INSTALL_PREFIX}/scripts\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_RUNTIME_INCDIR=\"${CMAKE_INSTALL_PREFIX}/include\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DCFG_RUNTIME_DOCDIR=\"${CMAKE_INSTALL_PREFIX}/share/man\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_LIBRARY=\"${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_PACKAGE_PATH=\"${CMAKE_INSTALL_PREFIX}/lib\"")


IF(APPLE)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DMAC_OSX_TCL=1")
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_COREFOUNDATION=1")
	ENDIF(COREFOUNDATION_FRAMEWORK)
ENDIF(APPLE)
#----------------------------------------------------------------------------
# Tom's math library - should MP_PREC be an option?
OPTION(TCL_TOMMATH "Enable Tcl Tom's Math Library" ON)
IF(TCL_TOMMMATH)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_TOMMATH=1")
   SET(TCL_CFLAGS "${TCL_CFLAGS} -DMP_PREC=4")
ENDIF(TCL_TOMMMATH)

#----------------------------------------------------------------------------
SC_MISSING_POSIX_HEADERS()

SC_ENABLE_THREADS()


SC_TCL_CFG_ENCODING()

SC_TCL_LINK_LIBS()

# TODO - look over SC_ENABLE_SHARED, SC_CONFIG_CFLAGS and SC_ENABLE_SYMBOLS
# need to decide how much of that logic is needed

SC_TCL_64BIT_FLAGS()

# Test endianness
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
IF(WORDS_BIGENDIAN)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DWORDS_BIGENDIAN=1")
ENDIF(WORDS_BIGENDIAN)

# POSIX substitutes
CHECK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
IF(NOT HAVE_GETCW)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DUSEGETWD=1") 
ENDIF(NOT HAVE_GETCW)
CHECK_FUNCTION_EXISTS(getwd HAVE_GETWD)
IF(NOT HAVE_GETWD)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_GETWD=1") 
ENDIF(NOT HAVE_GETWD)
CHECK_FUNCTION_EXISTS(opendir HAVE_OPENDIR)
IF(NOT HAVE_OPENDIR)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/opendir.c)
ENDIF(NOT HAVE_OPENDIR)
CHECK_FUNCTION_EXISTS(strtol HAVE_STRTOL)
IF(NOT HAVE_STRTOL)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/strtol.c)
ENDIF(NOT HAVE_STRTOL)
CHECK_FUNCTION_EXISTS(waitpid HAVE_WAITPID)
IF(NOT HAVE_WAITPID)
	SET(COMPAT_SRCS ${COMPAT_SRCS} compat/waitpid.c)
ENDIF(NOT HAVE_WAITPID)
CHECK_FUNCTION_EXISTS(strerror HAVE_STRERROR)
IF(NOT HAVE_STRERROR)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_STRERROR=1") 
ENDIF(NOT HAVE_STRERROR)
CHECK_FUNCTION_EXISTS_D(wait3 HAVE_WAIT3)
IF(NOT HAVE_WAIT3)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_WAIT3=1") 
ENDIF(NOT HAVE_WAIT3)
CHECK_FUNCTION_EXISTS_D(uname HAVE_UNAME)
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

CHECK_FUNCTION_EXISTS(getaddrinfo HAVE_GETADDRINFO)
IF(HAVE_GETADDRINFO)
		  CHECK_GETADDERINFO_WORKING_D()
ENDIF(HAVE_GETADDRINFO)


# Thread safe tests - need to go over macros and
# get the right logic going here
IF(TCL_THREADS)
	CHECK_FUNCTION_EXISTS_D(getpwuid HAVE_GETPWUID_R)
	IF(HAVE_GETPWUID_R)
		SET(HAVE_GETPWUID_R_5_SRC "
		#include <sys/types.h>
		#include <pwd.h>
		int main(){
		uid_t uid;
		struct passwd pw, *pwp;
		char buf[512];
		int buflen = 512;

		(void) getpwuid_r(uid, &pw, buf, buflen, &pwp);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETPWUID_R_5_SRC}"  HAVE_GETPWUID_R_5)
		IF(HAVE_GETPWUID_R_5)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETPWUID_R_5=1")
		ELSE(HAVE_GETPWUID_R_5)
			SET(HAVE_GETPWUID_R_4_SRC "
			#include <sys/types.h>
			#include <pwd.h>
		   int main(){
			uid_t uid;
			struct passwd pw;
			char buf[512];
			int buflen = 512;

			(void) getpwuid_r(uid, &pw, buf, buflen);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETPWUID_R_4_SRC}" HAVE_GETPWUID_R_4)
			IF(HAVE_GETPWUID_R_4)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETPWUID_R_4=1")
			ENDIF(HAVE_GETPWUID_R_4)
		ENDIF(HAVE_GETPWUID_R_5)
	ENDIF(HAVE_GETPWUID_R)

	CHECK_FUNCTION_EXISTS_D(getpwnam HAVE_GETPWNAM_R)
	IF(HAVE_GETPWNAM_R)
		SET(HAVE_GETPWNAM_R_5_SRC "
		#include <sys/types.h>
		#include <pwd.h>
		int main(){
		char *name;
		struct passwd pw, *pwp;
		char buf[512];
		int buflen = 512;

		(void) getpwnam_r(name, &pw, buf, buflen, &pwp);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETPWNAM_R_5_SRC}"  HAVE_GETPWNAM_R_5)
		IF(HAVE_GETPWNAM_R_5)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETPWNAM_R_5=1")
		ELSE(HAVE_GETPWNAM_R_5)
			SET(HAVE_GETPWNAM_R_4_SRC "
			#include <sys/types.h>
			#include <pwd.h>
		   int main(){
			char *name;
			struct passwd pw;
			char buf[512];
			int buflen = 512;

			(void)getpwnam_r(name, &pw, buf, buflen);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETPWNAM_R_4_SRC}" HAVE_GETPWNAM_R_4)
			IF(HAVE_GETPWNAM_R_4)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETPWNAM_R_4=1")
			ENDIF(HAVE_GETPWNAM_R_4)
		ENDIF(HAVE_GETPWNAM_R_5)
	ENDIF(HAVE_GETPWNAM_R)

	CHECK_FUNCTION_EXISTS_D(getgrgid HAVE_GETGRGID_R)
	IF(HAVE_GETGRGID_R)
		SET(HAVE_GETGRGID_R_5_SRC "
		#include <sys/types.h>
		#include <grp.h>
		int main(){
		gid_t gid;
		struct group gr, *grp;
		char buf[512];
		int buflen = 512;

		(void) getgrgid_r(gid, &gr, buf, buflen, &grp);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETGRGID_R_5_SRC}"  HAVE_GETGRGID_R_5)
		IF(HAVE_GETGRGID_R_5)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETGRGID_R_5=1")
		ELSE(HAVE_GETGRGID_R_5)
			SET(HAVE_GETGRGID_R_4_SRC "
			#include <sys/types.h>
			#include <grp.h>
		   int main(){
			gid_t gid;
			struct group gr;
			char buf[512];
			int buflen = 512;

			(void)getgrgid_r(gid, &gr, buf, buflen);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETGRGID_R_4_SRC}" HAVE_GETGRGID_R_4)
			IF(HAVE_GETGRGID_R_4)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETGRGID_R_4=1")
			ENDIF(HAVE_GETGRGID_R_4)
		ENDIF(HAVE_GETGRGID_R_5)
	ENDIF(HAVE_GETGRGID_R)


	CHECK_FUNCTION_EXISTS_D(getgrnam HAVE_GETGRNAM_R)
	IF(HAVE_GETGRNAMM_R)
		SET(HAVE_GETGRNAMM_R_5_SRC "
		#include <sys/types.h>
		#include <grp.h>
		int main(){
		char *name;
		struct group gr, *grp;
		char buf[512];
		int buflen = 512;

		(void) getgrnam_r(name, &gr, buf, buflen, &grp);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETGRNAMM_R_5_SRC}"  HAVE_GETGRNAMM_R_5)
		IF(HAVE_GETGRNAMM_R_5)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETGRNAMM_R_5=1")
		ELSE(HAVE_GETGRNAMM_R_5)
			SET(HAVE_GETGRNAMM_R_4_SRC "
			#include <sys/types.h>
			#include <grp.h>
		   int main(){
			char *name;
			struct group gr;
			char buf[512];
			int buflen = 512;

			(void)getgrnam_r(name, &gr, buf, buflen);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETGRNAMM_R_4_SRC}" HAVE_GETGRNAMM_R_4)
			IF(HAVE_GETGRNAMM_R_4)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETGRNAMM_R_4=1")
			ENDIF(HAVE_GETGRNAMM_R_4)
		ENDIF(HAVE_GETGRNAMM_R_5)
	ENDIF(HAVE_GETGRNAMM_R)

	IF(NOT HAVE_GETHOSTBYNAME)
		CHECK_LIBRARY(GHBN lnsl gethostbyname)
		SET(HAVE_GETHOSTBYNAME ${GHBN})
	ENDIF(NOT HAVE_GETHOSTBYNAME)

	CHECK_FUNCTION_EXISTS_D(gethostbyname HAVE_GETHOSTBYNAME_R)
	IF(HAVE_GETHOSTBYNAME_R)
		SET(HAVE_GETHOSTBYNAME_R_6_SRC "
		#include <netdb.h>
		int main(){
		char *name;
		struct hostent *he, *res;
		char buffer[2048];
		int buflen = 2048;
		int h_errnop;

		(void) gethostbyname_r(name, he, buffer, buflen, &res, &h_errnop);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYNAME_R_6_SRC}"  HAVE_GETHOSTBYNAME_R_5)
		IF(HAVE_GETHOSTBYNAME_R_6)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETHOSTBYNAME_R_6=1")
		ELSE(HAVE_GETHOSTBYNAME_R_6)
			SET(HAVE_GETHOSTBYNAME_R_5_SRC "
			#include <netdb.h>
		   int main(){
			char *name;
			struct hostent *he;
			char buffer[2048];
			int buflen = 2048;
			int h_errnop;

			(void) gethostbyname_r(name, he, buffer, buflen, &h_errnop);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYNAME_R_5_SRC}"  HAVE_GETHOSTBYNAME_R_5)

			IF(HAVE_GETHOSTBYNAME_R_5)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETHOSTBYNAME_R_5=1")
			ELSE(HAVE_GETHOSTBYNAME_R_5)
				SET(HAVE_GETHOSTBYNAME_R_3_SRC "
				#include <netdb.h>
		      int main(){
				char *name;
				struct hostent *he;
				struct hostent_data data;

				(void) gethostbyname_r(name, he, &data);
		      return 0;}
				")
				CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYNAME_R_3_SRC}" HAVE_GETHOSTBYNAME_R_3)
				IF(HAVE_GETHOSTBYNAME_R_3)
					SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETHOSTBYNAME_R_3=1")
				ENDIF(HAVE_GETHOSTBYNAME_R_3)
			ENDIF(HAVE_GETHOSTBYNAME_R_5)
		ENDIF(HAVE_GETHOSTBYNAME_R_6)
	ENDIF(HAVE_GETHOSTBYNAME_R)


	CHECK_FUNCTION_EXISTS_D(gethostbyaddr HAVE_GETHOSTBYADDR_R)
	IF(HAVE_GETHOSTBYADDR_R)
		SET(HAVE_GETHOSTBYADDR_R_7_SRC "
		#include <netdb.h>
		int main(){
		char *addr;
		int length;
		int type;
		struct hostent *result;
		char buffer[2048];
		int buflen = 2048;
		int h_errnop;

		(void) gethostbyaddr_r(addr, length, type, result, buffer, buflen, &h_errnop);
		return 0;}
		")
		CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYADDR_R_7_SRC}"  HAVE_GETHOSTBYADDR_R_7)
		IF(HAVE_GETHOSTBYADDR_R_7)
			SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETHOSTBYADDR_R_7=1")
		ELSE(HAVE_GETHOSTBYADDR_R_7)
			SET(HAVE_GETHOSTBYADDR_R_8_SRC "
			#include <netdb.h>
		   int main(){
			char *addr;
			int length;
			int type;
			struct hostent *result, *resultp;
			char buffer[2048];
			int buflen = 2048;
			int h_errnop;

			(void) gethostbyaddr_r(addr, length, type, result, buffer, buflen, &resultp, &h_errnop);
		   return 0;}
			")
			CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYADDR_R_8_SRC}" HAVE_GETHOSTBYADDR_R_8)
			IF(HAVE_GETHOSTBYADDR_R_8)
				SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_GETHOSTBYADDR_R_8=1")
			ENDIF(HAVE_GETHOSTBYADDR_R_8)
		ENDIF(HAVE_GETHOSTBYADDR_R_7)
	ENDIF(HAVE_GETHOSTBYADDR_R)


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

CHECK_INCLUDE_FILE_D(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE_D(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE_D(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
   SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_FSTATFS=1")
ENDIF(NOT HAVE_SYS_FSTATFS_H)
CHECK_INCLUDE_FILE_D(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE_D(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE_D(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE_D(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE_D(unistd.h HAVE_UNISTD_H)





CHECK_COMPILER_SUPPORTS_HIDDEN_D()

CHECK_LIBRARY(DL dl dlopen)



CHECK_INCLUDE_FILE_USABILITY_D(sys/modem.h SYS_MODEM_H)


CHECK_FD_SET_IN_TYPES_D()

CHECK_INCLUDE_FILE_D(copyfile.h HAVE_COPYFILE_H)
CHECK_FUNCTION_EXISTS_D(copyfile HAVE_COPYFILE)



CHECK_TYPE_SIZE_D(blkcnt_t HAVE_BLKCNT_T)
CHECK_TYPE_SIZE_D(intptr_t HAVE_INTPTR_T)
CHECK_TYPE_SIZE_D(uintptr_t HAVE_UINTPTR_T)
CHECK_FUNCTION_EXISTS_D(getattrlist HAVE_GETATTRLIST)
CHECK_INCLUDE_FILE_D(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
CHECK_FUNCTION_EXISTS_D(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
CHECK_FUNCTION_EXISTS_D(pthread_atfork HAVE_PTHREAD_ATFORK)

IF(APPLE)
	# Now that all the tests are done, configure the tclConfig.h file:
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unix/tclConfig.h.in ${TCL_BINARY_DIR}/include/tclConfig.h)
ENDIF(APPLE)

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
	generic/tclIO.c
	generic/tclIOCmd.c
	generic/tclIOGT.c
	generic/tclIORChan.c
	generic/tclIOSock.c
	generic/tclIOUtil.c
	generic/tclIndexObj.c
	generic/tclInterp.c
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
	generic/tclStrToD.c
	generic/tclStringObj.c
	generic/tclStubInit.c
	generic/tclThread.c
	generic/tclThreadAlloc.c
	generic/tclThreadJoin.c
	generic/tclThreadStorage.c
	generic/tclTimer.c
	generic/tclTomMathInterface.c
	generic/tclTrace.c
	generic/tclUniData.c
	generic/tclUtf.c
	generic/tclUtil.c
	generic/tclVar.c
)

SET(TCL_STUB_SRCS
	generic/tclStubLib.c
)

SET(TCL_TOMMATH_SRCS
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
	libtommath/bn_mp_div_2.c
	libtommath/bn_mp_div_2d.c
	libtommath/bn_mp_div_3.c
	libtommath/bn_mp_div_d.c
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
	libtommath/bn_reverse.c
	libtommath/bn_s_mp_add.c
	libtommath/bn_s_mp_mul_digs.c
	libtommath/bn_s_mp_sqr.c
	libtommath/bn_s_mp_sub.c
	libtommath/bncore.c
)

SET(TCL_OSX_SRCS
	macosx/tclMacOSXBundle.c
	macosx/tclMacOSXFCmd.c
	macosx/tclMacOSXNotify.c
)

SET(TCL_WIN_SRCS
	win/cat.c
	win/nmakehlp.c
	win/stub16.c
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
	win/tclWinTest.c
	win/tclWinThrd.c
	win/tclWinTime.c
)

SET(TCL_UNIX_SRCS
	unix/tclAppInit.c
	unix/tclUnixChan.c
	unix/tclUnixCompat.c
	unix/tclUnixEvent.c
	unix/tclUnixFCmd.c
	unix/tclUnixFile.c
	unix/tclUnixInit.c
	unix/tclUnixPipe.c
	unix/tclUnixSock.c
	unix/tclUnixTest.c
	unix/tclUnixThrd.c
	unix/tclUnixTime.c
)

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

SET(TCL_SRCS ${TCL_GENERIC_SRCS} ${TCL_TOMMATH_SRCS} ${TCL_NOTIFY_SRCS} ${TCL_STUB_SRCS}) 
IF(WIN32)
	SET(TCL_SRCS ${TCL_SRCS} ${TCL_WIN_SRCS})
ELSE(WIN32)
	SET(TCL_SRCS ${TCL_SRCS} ${TCL_UNIX_SRCS})
	IF(APPLE)
		SET(TCL_SRCS ${TCL_SRCS} ${TCL_OSX_SRCS} unix/tclLoadDyld.c)
	ENDIF(APPLE)
	IF(NOT APPLE)
		SET(TCL_SRCS ${TCL_SRCS} unix/tclLoadDl.c)
	ENDIF(NOT APPLE)
ENDIF(WIN32)

SET(TCL_INCLUDE_PATH ${TCL_SOURCE_DIR}/generic ${TCL_SOURCE_DIR}/libtommath ${TCL_BINARY_DIR}/include)
IF(WIN32)
	SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_SOURCE_DIR}/win)
ELSE(WIN32)
	SET(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_SOURCE_DIR}/unix)
ENDIF(WIN32)

include_directories(
	${TCL_INCLUDE_PATH}
)

IF(WIN32)
	SET(TCLSH_SRCS win/tclAppInit.c)
ELSE(WIN32)
	SET(TCLSH_SRCS unix/tclAppInit.c)
ENDIF(WIN32)

MESSAGE("TCL_CFLAGS: ${TCL_CFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TCL_CFLAGS}")

add_library(tcl ${TCL_SRCS})
target_link_libraries(tcl ${COREFOUNDATION_FRAMEWORK} ${TCL_LINK_LIBS} ${DL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS tcl DESTINATION lib)
SET_TARGET_PROPERTIES(tcl PROPERTIES VERSION ${TCL_VERSION} SOVERSION ${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

add_library(tclstub STATIC ${TCL_STUB_SRCS})
target_link_libraries(tclstub ${CMAKE_THREAD_LIBS_INIT} ${COREFOUNDATION_FRAMEWORK} ${M_LIBRARY} ${DL_LIBRARY})
install(TARGETS tclstub DESTINATION lib)

add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl)
install(TARGETS tclsh DESTINATION bin)

install(FILES library/init.tcl DESTINATION lib/tcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
