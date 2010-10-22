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

# build shared libs by default
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)

# build static libs by default
OPTION(BUILD_STATIC_LIBS "Build static libraries" ON)

# version numbers
SET(TCL_MAJOR_VERSION 8)
SET(TCL_MINOR_VERSION 5)
SET(TCL_PATCH_LEVEL 9)

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
# Unlike the autotools based builds, CMake will always use tclConfig.h - define
# this at the toplevel so tclPort.h includes it.

add_definitions(
	-DHAVE_TCL_CONFIG_H
)

# We will need a tclConfig.h.in file to hold all the #cmakedefine statements,
# which will in turn be used to generate a tclConfig.h file.  To succeed, the 
# file must first be present, which we take care of here:

SET(CONFIG_H_FILE "${CMAKE_BINARY_DIR}/include/tclConfig.h.in")
FILE(WRITE ${CONFIG_H_FILE} "/**** Tcl define statements from CMake ****/\n")
FILE(APPEND ${CONFIG_H_FILE} "#ifndef _TCLCONFIG\n")
FILE(APPEND ${CONFIG_H_FILE} "#define _TCLCONFIG\n")

INCLUDE(${TCL_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_NAME \"tcl\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_TARNAME \"tcl\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_VERSION \"${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_STRING \"tcl ${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define PACKAGE_BUGREPORT \"\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TCL_SHLIB_EXT \"${CMAKE_SHARED_LIBRARY_SUFFIX}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_LIBDIR \"${CMAKE_INSTALL_PREFIX}/lib\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_BINDIR \"${CMAKE_INSTALL_PREFIX}/bin\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_SCRDIR \"${CMAKE_INSTALL_PREFIX}/scripts\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_INCDIR \"${CMAKE_INSTALL_PREFIX}/include\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_INSTALL_DOCDIR \"${CMAKE_INSTALL_PREFIX}/share/man\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_LIBDIR \"${CMAKE_INSTALL_PREFIX}/lib\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_BINDIR \"${CMAKE_INSTALL_PREFIX}/bin\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_SCRDIR \"${CMAKE_INSTALL_PREFIX}/scripts\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_INCDIR \"${CMAKE_INSTALL_PREFIX}/include\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define CFG_RUNTIME_DOCDIR \"${CMAKE_INSTALL_PREFIX}/share/man\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TCL_LIBRARY \"${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}\"\n")
FILE(APPEND ${CONFIG_H_FILE} "#define TCL_PACKAGE_PATH \"${CMAKE_INSTALL_PREFIX}/lib\"\n")

IF(NOT TCL_CFGVAL_ENCODING)
	SET(TCL_CFGVAL_ENCODING "iso8859-1")
ENDIF(NOT TCL_CFGVAL_ENCODING)
FILE(APPEND ${CONFIG_H_FILE} "#define TCL_CFGVAL_ENCODING \"${TCL_CFGVAL_ENCODING}\"\n")


IF(APPLE)
	FILE(APPEND ${CONFIG_H_FILE} "#define MAC_OSX_TCL 1\n")
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_COREFOUNDATION 1\n")
	ENDIF(COREFOUNDATION_FRAMEWORK)
ENDIF(APPLE)
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


# Tom's math library
OPTION(TCL_TOMMATH "Enable Tcl Tom's Math Library" ON)
FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_TOMMATH 1\n")

#----------------------------------------------------------------------------
# Run tests for system headers and functionality
CHECK_INCLUDE_FILE_H(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE_H(sys/stat.h HAVE_SYS_STAT_H)
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

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)

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

# Now that all the tests are done, configure the tclConfig.h file:
FILE(APPEND ${CONFIG_H_FILE} "#endif /*_TCLCONFIG*/\n")
configure_file(${CONFIG_H_FILE} ${TCL_BINARY_DIR}/include/tclConfig.h)

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
	IF(APPLE)
		SET(TCL_SRCS ${TCL_SRCS} ${TCL_OSX_SRCS})
	ENDIF(APPLE)
	SET(TCL_SRCS ${TCL_SRCS} ${TCL_UNIX_SRCS} unix/tclLoadDyld.c)
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

add_library(tcl ${TCL_SRCS})
target_link_libraries(tcl ${CMAKE_THREAD_LIBS_INIT} ${COREFOUNDATION_FRAMEWORK})
install(TARGETS tcl DESTINATION lib)
SET(TCL_LIBRARY tcl)

add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl)
install(TARGETS tclsh DESTINATION bin)
SET(TCL_TCLSH tclsh)
