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
# The header structure of Tcl/Tk ends up requiring compile-line definitions of
# many variables to ensure the various headers get the correct values.  Due
# to the number of these flags, we build up a TCL_CFLAGS list to be passed
# to the final build.  Unlike Tk, Tcl also has a pre-defined tclConfig.h.in
# file that needs to be configured - handle both.

INCLUDE(${TCL_SOURCE_DIR}/CMake/CheckSystemFunctionality.cmake)

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

IF(NOT TCL_CFGVAL_ENCODING)
	SET(TCL_CFGVAL_ENCODING "iso8859-1")
ENDIF(NOT TCL_CFGVAL_ENCODING)
SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_CFGVAL_ENCODING=\"${TCL_CFGVAL_ENCODING}\"")


IF(APPLE)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DMAC_OSX_TCL=1")
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DHAVE_COREFOUNDATION=1")
	ENDIF(COREFOUNDATION_FRAMEWORK)
ENDIF(APPLE)
#----------------------------------------------------------------------------
# Define a number of variables to be controlled by options
OPTION(TCL_THREADS "Enable Tcl Thread support" ON)
IF(TCL_THREADS)
   SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_THREADS=1")
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DUSE_THREAD_ALLOC=1")
	SET(TCL_CFLAGS "${TCL_CFLAGS} -D_REENTRANT=1")
	SET(TCL_CFLAGS "${TCL_CFLAGS} -D_THREAD_SAFE=1")
	IF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
		SET(TCL_CFLAGS "${TCL_CFLAGS} -D_POSIX_PTHREAD_SEMANTICS=1")
	ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
ENDIF(TCL_THREADS)


# Tom's math library
OPTION(TCL_TOMMATH "Enable Tcl Tom's Math Library" ON)
IF(TCL_TOMMMATH)
	SET(TCL_TOMMATH "${TCL_CFLAGS} -DTCL_TOMMATH=1")
ENDIF(TCL_TOMMMATH)

#----------------------------------------------------------------------------
# Run tests for system headers and functionality
CHECK_INCLUDE_FILE_D(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE_D(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE_D(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
   SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_FSTATFS=1")
ENDIF(NOT HAVE_SYS_FSTATFS_H)
CHECK_INCLUDE_FILE_D(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE_D(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE_D(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE_D(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE_D(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE_D(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE_D(unistd.h HAVE_UNISTD_H)

CHECK_INCLUDE_FILE_D(dirent.h HAVE_DIRENT_H)
IF(NOT HAVE_DIRENT_H)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_DIRENT_H=1")
ENDIF(NOT HAVE_DIRENT_H)

CHECK_INCLUDE_FILE_USABILITY_D(float.h FLOAT_H)
CHECK_INCLUDE_FILE_USABILITY_D(values.h VALUES_H)
CHECK_INCLUDE_FILE_USABILITY_D(limits.h LIMITS_H)
CHECK_INCLUDE_FILE_USABILITY_D(sys/wait.h SYS_WAIT_H)
CHECK_INCLUDE_FILE_USABILITY_D(dlfcn.h DLFCN_H)
CHECK_INCLUDE_FILE_USABILITY_D(sys/param.h SYS_PARAM_H)


CHECK_LIBRARY(M m sin)
CHECK_LIBRARY(IEEE ieee main)
CHECK_LIBRARY(INET inet main)

CHECK_INCLUDE_FILE_USABILITY_D(net/errno.h NET_ERRNO_H)

CHECK_FUNCTION_EXISTS(connect HAVE_CONNECT)
CHECK_FUNCTION_EXISTS(gethostbyname HAVE_GETHOSTBYNAME)
IF(NOT HAVE_GETHOSTBYNAME)
	CHECK_LIBRARY(GHBN lnsl gethostbyname)
	SET(HAVE_GETHOSTBYNAME ${GHBN})
ENDIF(NOT HAVE_GETHOSTBYNAME)

CHECK_COMPILER_SUPPORTS_HIDDEN_D()

CHECK_LIBRARY(DL dl dlopen)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)

CHECK_FUNCTION_EXISTS_D(getcwd HAVE_GETCWD)
CHECK_FUNCTION_EXISTS_D(opendir HAVE_OPENDIR)
CHECK_FUNCTION_EXISTS_D(strtol HAVE_STRTOL)
CHECK_FUNCTION_EXISTS_D(waitpid HAVE_WAITPID)
CHECK_FUNCTION_EXISTS_D(strerror HAVE_STRERROR)
CHECK_FUNCTION_EXISTS_D(getwd HAVE_GETWD)
CHECK_FUNCTION_EXISTS_D(wait3 HAVE_WAIT3)
CHECK_FUNCTION_EXISTS_D(uname HAVE_UNAME)
CHECK_FUNCTION_EXISTS_D(realpath HAVE_REALPATH)
CHECK_FUNCTION_EXISTS(getaddrinfo HAVE_GETADDRINFO)
IF(HAVE_GETADDRINFO)
		  CHECK_GETADDERINFO_WORKING_D()
ENDIF(HAVE_GETADDRINFO)


CHECK_INCLUDE_FILE_USABILITY_D(sys/modem.h SYS_MODEM_H)

TERMIOS_TERMIO_SGTTY()

CHECK_FD_SET_IN_TYPES_D()

CHECK_INCLUDE_FILE_USABILITY_D(sys/time.h HAVE_SYS_TIME_H)
CHECK_TIME_AND_SYS_TIME()

CHECK_FUNCTION_EXISTS_D(gmtime_r HAVE_GMTIME_R)
CHECK_FUNCTION_EXISTS_D(localtime_r HAVE_LOCALTIME_R)
CHECK_FUNCTION_EXISTS_D(mktime HAVE_MKTIME)

CHECK_STRUCT_HAS_MEMBER_D("struct tm" tm_tzadj time.h HAVE_TM_TZADJ)
CHECK_STRUCT_HAS_MEMBER_D("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)

CHECK_INCLUDE_FILE_D(copyfile.h HAVE_COPYFILE_H)
CHECK_FUNCTION_EXISTS_D(copyfile HAVE_COPYFILE)

CHECK_INCLUDE_FILE_D(sys/ioctl.h HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILE_D(sys/filio.h HAVE_SYS_FILIO_H)



CHECK_FUNCTION_EXISTS_D(getpwuid HAVE_GETPWUID_R)
CHECK_FUNCTION_EXISTS_D(getpwnam HAVE_GETPWNAM_R)
CHECK_FUNCTION_EXISTS_D(getgrgid HAVE_GETGRGID_R)
CHECK_FUNCTION_EXISTS_D(getgrnam HAVE_GETGRNAM_R)
CHECK_TYPE_SIZE_D(blkcnt_t HAVE_BLKCNT_T)
CHECK_TYPE_SIZE_D(intptr_t HAVE_INTPTR_T)
CHECK_TYPE_SIZE_D(uintptr_t HAVE_UINTPTR_T)
CHECK_FUNCTION_EXISTS_D(getattrlist HAVE_GETATTRLIST)
CHECK_INCLUDE_FILE_D(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
CHECK_FUNCTION_EXISTS_D(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
CHECK_FUNCTION_EXISTS_D(pthread_atfork HAVE_PTHREAD_ATFORK)
#CHECK_FUNCTION_EXISTS_D( HAVE_MTSAFE_GETHOSTBYNAME)
#CHECK_FUNCTION_EXISTS_D( HAVE_MTSAFE_GETHOSTBYADDR)
#CHECK_FUNCTION_EXISTS_D( HAVE_STRUCT_STAT_ST_BLOCKS)
#CHECK_FUNCTION_EXISTS_D( HAVE_STRUCT_STAT_ST_BLKSIZE)
#CHECK_FUNCTION_EXISTS_D( HAVE_GETPWUID_R_5)
#CHECK_FUNCTION_EXISTS_D( HAVE_GETPWNAM_R_5)
#CHECK_FUNCTION_EXISTS_D( HAVE_GETGRGID_R_5)
#CHECK_FUNCTION_EXISTS_D( HAVE_GETGRNAM_R_5)
#CHECK_INCLUDE_FILE_D( USE_VFORK)
#CHECK_INCLUDE_FILE_D( TCL_WIDE_INT_TYPE long\ long)
#CHECK_INCLUDE_FILE_D( TCL_CFG_DEBUG)
#CHECK_INCLUDE_FILE_D( USE_TERMIOS)
#CHECK_INCLUDE_FILE_D( TIME_WITH_SYS_TIME)
#CHECK_INCLUDE_FILE_D( HAVE_TM_GMTOFF)
#CHECK_INCLUDE_FILE_D( HAVE_TIMEZONE_VAR)
#CHECK_INCLUDE_FILE_D( HAVE_STRUCT_STAT64)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine M_PREC 4)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_DEFAULT_ENCODING \"utf-8\"")
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_LOAD_FROM_MEMORY 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_WIDE_CLICKS 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_AVAILABILITYMACROS_H 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_WEAK_IMPORT 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine _DARWIN_C_SOURCE 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_FTS 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine TCL_UNLOAD_DLLS 1)
#FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine USE_DTRACE 1)

FIND_PACKAGE(Threads)

# Now that all the tests are done, configure the tclConfig.h file:
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unix/tclConfig.h.in ${TCL_BINARY_DIR}/include/tclConfig.h)

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
target_link_libraries(tcl ${COREFOUNDATION_FRAMEWORK} ${M_LIBRARY} ${DL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS tcl DESTINATION lib)
SET_TARGET_PROPERTIES(tcl PROPERTIES VERSION ${TCL_VERSION} SOVERSION ${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})

add_library(tclstub STATIC ${TCL_STUB_SRCS})
target_link_libraries(tclstub ${CMAKE_THREAD_LIBS_INIT} ${COREFOUNDATION_FRAMEWORK} ${M_LIBRARY} ${DL_LIBRARY})
install(TARGETS tclstub DESTINATION lib)

add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl)
install(TARGETS tclsh DESTINATION bin)

install(FILES library/init.tcl DESTINATION lib/tcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION})
