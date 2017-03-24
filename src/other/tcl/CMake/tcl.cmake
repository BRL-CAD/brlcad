# CMake version of the tcl.m4 logic, insofar as it maps to CMake and has been
# needed.

INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckPrototypeExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCSourceRuns)

INCLUDE(ac_std_funcs)

MACRO(ADD_TCL_CFLAG TCL_CFLAG)
	add_definitions(-D${TCL_CFLAG}=1)
ENDMACRO(ADD_TCL_CFLAG)

MACRO(CHECK_C_FLAG flag)
   STRING(TOUPPER ${flag} UPPER_FLAG)
   STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
   STRING(REGEX REPLACE "=" "_" UPPER_FLAG ${UPPER_FLAG})
   IF(${ARGC} LESS 2)
      CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
   ELSE(${ARGC} LESS 2)
      IF(NOT ${ARGV1})
         CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
         IF(${UPPER_FLAG}_COMPILER_FLAG)
            MESSAGE("- Found ${ARGV1} - setting to -${flag}")
            SET(${ARGV1} "-${flag}" CACHE STRING "${ARGV1}" FORCE)
         ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
      ENDIF(NOT ${ARGV1})
   ENDIF(${ARGC} LESS 2)
   IF(${UPPER_FLAG}_COMPILER_FLAG)
      SET(${UPPER_FLAG}_COMPILER_FLAG "-${flag}")
   ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

# Wrappers for include file checks
MACRO(TCL_CHECK_INCLUDE_FILE filename var)
	CHECK_INCLUDE_FILE(${filename} ${var})
	IF(${var})
		add_definitions(-D${var}=1)
	ENDIF(${var})
ENDMACRO(TCL_CHECK_INCLUDE_FILE)

MACRO(TCL_CHECK_INCLUDE_FILE_USABILITY filename var)
	CHECK_INCLUDE_FILE(${filename} HAVE_${var})
	IF(HAVE_${var})
		SET(HEADER_SRC "
#include <${filename}>
main(){};
		")
		CHECK_C_SOURCE_COMPILES("${HEADER_SRC}" ${var}_USABLE)
	ENDIF(HAVE_${var})
	IF(NOT HAVE_${var} OR NOT ${var}_USABLE)
		add_definitions(-DNO_${var}=1)
	ELSE(NOT HAVE_${var} OR NOT ${var}_USABLE)
		add_definitions(-DHAVE_${var}=1)
	ENDIF(NOT HAVE_${var} OR NOT ${var}_USABLE)
ENDMACRO(TCL_CHECK_INCLUDE_FILE_USABILITY filename var)

# Wrapper for function testing
MACRO(TCL_CHECK_FUNCTION_EXISTS function var)
	CHECK_FUNCTION_EXISTS(${function} ${var})
	IF(${var})
		add_definitions(-D${var}=1)
	ENDIF(${var})
ENDMACRO(TCL_CHECK_FUNCTION_EXISTS)

# Check sizes (accepts extra headers)
MACRO(TCL_CHECK_TYPE_SIZE typename var)
   FOREACH(arg ${ARGN})
      SET(headers ${headers} ${arg})
   ENDFOREACH(arg ${ARGN})
   SET(CHECK_EXTRA_INCLUDE_FILES ${headers})
   CHECK_TYPE_SIZE(${typename} HAVE_${var}_T)
   SET(CHECK_EXTRA_INCLUDE_FILES)
   IF(HAVE_${var}_T)
      add_definitions(-DHAVE_${var}_T=1)
      add_definitions(-DSIZEOF${var}=${HAVE_${var}_T})
   ENDIF(HAVE_${var}_T)
ENDMACRO(TCL_CHECK_TYPE_SIZE)

# Check for a member of a structure
MACRO(TCL_CHECK_STRUCT_HAS_MEMBER structname member header var)
   CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} HAVE_${var})
   IF(HAVE_${var})
      add_definitions(-DHAVE_${var}=1)
   ENDIF(HAVE_${var})
ENDMACRO(TCL_CHECK_STRUCT_HAS_MEMBER)



# Note - for these path and load functions, should move the FindTCL.cmake
# logic that applies to here

#------------------------------------------------------------------------
# SC_PATH_TCLCONFIG
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_PATH_TKCONFIG
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_LOAD_TCLCONFIG
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_LOAD_TKCONFIG
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_PROG_TCLSH
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_BUILD_TCLSH
#------------------------------------------------------------------------

# TODO

#------------------------------------------------------------------------
# SC_ENABLE_SHARED
#------------------------------------------------------------------------

# This will probably be handled by CMake variables rather than a
# specific SC command

#------------------------------------------------------------------------
# SC_ENABLE_FRAMEWORK
#------------------------------------------------------------------------

# Not immediately clear how this will work in CMake


#------------------------------------------------------------------------
# SC_ENABLE_THREADS
#------------------------------------------------------------------------
MACRO(SC_ENABLE_THREADS)
   FIND_PACKAGE(Threads)
   IF(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")
		SET(TCL_FOUND_THREADS ON)
   ELSE(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")
		SET(TCL_FOUND_THREADS OFF)
   ENDIF(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")
	OPTION(TCL_THREADS "Enable Tcl Thread support" ${TCL_FOUND_THREADS})
   IF(TCL_THREADS)
		ADD_TCL_CFLAG(TCL_THREADS)
		ADD_TCL_CFLAG(USE_THREAD_ALLOC)
		ADD_TCL_CFLAG(_REENTRANT)
		ADD_TCL_CFLAG(_THREAD_SAFE)
	   SET(TCL_THREADS_LIB ${CMAKE_THREAD_LIBS_INIT})
		IF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
			ADD_TCL_CFLAG(_POSIX_PTHREAD_SEMANTICS)
		ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
		CHECK_FUNCTION_EXISTS(pthread_attr_setstacksize HAVE_PTHREAD_ATTR_SETSTACKSIZE)
		IF(HAVE_PTHREAD_ATTR_SETSTACKSIZE)
			ADD_TCL_CFLAG(HAVE_PTHREAD_ATTR_SETSTACKSIZE)
		ENDIF(HAVE_PTHREAD_ATTR_SETSTACKSIZE)
		CHECK_FUNCTION_EXISTS(pthread_attr_get_np HAVE_PTHREAD_ATTR_GET_NP)
		CHECK_FUNCTION_EXISTS(pthread_getattr_np HAVE_PTHREAD_GETATTR_NP)
		IF(HAVE_PTHREAD_ATTR_GET_NP)
			ADD_TCL_CFLAG(HAVE_PTHREAD_ATTR_GET_NP)
		ELSEIF(HAVE_PTHREAD_GETATTR_NP)
			ADD_TCL_CFLAG(HAVE_PTHREAD_GETATTR_NP)
		ENDIF(HAVE_PTHREAD_ATTR_GET_NP)
		IF(NOT HAVE_PTHREAD_ATTR_GET_NP AND NOT HAVE_PTHREAD_GETATTR_NP)
			CHECK_FUNCTION_EXISTS(pthread_get_stacksize_np HAVE_PTHREAD_GET_STACKSIZE_NP)
			IF(HAVE_PTHREAD_GET_STACKSIZE_NP)
				ADD_TCL_CFLAG(HAVE_PTHREAD_GET_STACKSIZE_NP)
			ENDIF(HAVE_PTHREAD_GET_STACKSIZE_NP)
		ENDIF(NOT HAVE_PTHREAD_ATTR_GET_NP AND NOT HAVE_PTHREAD_GETATTR_NP)
	ENDIF(TCL_THREADS)
ENDMACRO(SC_ENABLE_THREADS)

#------------------------------------------------------------------------
# SC_ENABLE_SYMBOLS
#------------------------------------------------------------------------

# TODO - this may be replaced by other CMake mechanisms

#------------------------------------------------------------------------
# SC_ENABLE_LANGINFO
#------------------------------------------------------------------------
MACRO(SC_ENABLE_LANGINFO)
	set(TCL_ENABLE_LANGINFO ON CACHE STRING "Trigger use of nl_langinfo if available.")
	IF(TCL_ENABLE_LANGINFO)
		CHECK_INCLUDE_FILE(langinfo.h HAVE_LANGINFO)
		IF(HAVE_LANGINFO)
			SET(LANGINFO_SRC "
			#include <langinfo.h>
			int main() {
			nl_langinfo(CODESET);
			}
			")
			CHECK_C_SOURCE_COMPILES("${LANGINFO_SRC}" LANGINFO_COMPILES)
			IF(LANGINFO_COMPILES)
				ADD_TCL_CFLAG(HAVE_LANGINFO)
			ELSE(LANGINFO_COMPILES)
				SET(TCL_ENABLE_LANGINFO OFF CACHE STRING "Langinfo not available" FORCE)
			ENDIF(LANGINFO_COMPILES)
		ELSE(HAVE_LANGINFO)
			SET(TCL_ENABLE_LANGINFO OFF CACHE STRING "Langinfo not available" FORCE)
		ENDIF(HAVE_LANGINFO)
	ENDIF(TCL_ENABLE_LANGINFO)
ENDMACRO(SC_ENABLE_LANGINFO)

#--------------------------------------------------------------------
# SC_CONFIG_MANPAGES
#--------------------------------------------------------------------

# TODO

#--------------------------------------------------------------------
# SC_CONFIG_SYSTEM
#--------------------------------------------------------------------

# Replaced by CMake functionality


#--------------------------------------------------------------------
# SC_CONFIG_CFLAGS
#--------------------------------------------------------------------

# TODO - many of these are either automatically handled or handled
# elsewhere, but should still handle what we need to

#--------------------------------------------------------------------
# SC_SERIAL_PORT
#--------------------------------------------------------------------
MACRO(SC_SERIAL_PORT)
	SET(TERMIOS_SRC_1 "
#include <termios.h>
int main() {
struct termios t;
if (tcgetattr(0, &t) == 0) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
}
	return 1;
}
	")
	SET(TERMIOS_SRC_2 "
#include <termios.h>
#include <errno.h>

int main() {
struct termios t;
if (tcgetattr(0, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
	}
	return 1;
}
	")
	SET(TERMIO_SRC_1 "
#include <termio.h>
int main() {
struct termio t;
if (ioctl(0, TCGETA, &t) == 0) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
}
	return 1;
}
   ")
	SET(TERMIO_SRC_2 "
#include <termio.h>
#include <errno.h>

int main() {
struct termio t;
if (ioctl(0, TCGETA, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
	}
	return 1;
}
	")
	SET(SGTTY_SRC_1 "
#include <sgtty.h>
int main() {
struct sgttyb t;
if (ioctl(0, TIOCGETP, &t) == 0) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
}
	return 1;
}
   ")
	SET(SGTTY_SRC_2 "
#include <sgtty.h>
#include <errno.h>

int main() {
struct sgttyb t;
if (ioctl(0, TIOCGETP, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
	}
	return 1;
}
	")
	if(NOT DEFINED HAVE_TERMIOS)
	CHECK_C_SOURCE_RUNS("${TERMIOS_SRC_1}" HAVE_TERMIOS)
	IF(NOT HAVE_TERMIOS)
		CHECK_C_SOURCE_RUNS("${TERMIO_SRC_1}" HAVE_TERMIO)
	ENDIF(NOT HAVE_TERMIOS)
	IF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
		CHECK_C_SOURCE_RUNS("${SGTTY_SRC_1}" HAVE_SGTTY)
	ENDIF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
	IF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS AND NOT HAVE_SGTTY)
		CHECK_C_SOURCE_RUNS("${TERMIOS_SRC_2}" HAVE_TERMIOS)
		IF(NOT HAVE_TERMIOS)
			CHECK_C_SOURCE_RUNS("${TERMIO_SRC_2}" HAVE_TERMIO)
		ENDIF(NOT HAVE_TERMIOS)
		IF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
			CHECK_C_SOURCE_RUNS("${SGTTY_SRC_2}" HAVE_SGTTY)
		ENDIF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
	ENDIF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS AND NOT HAVE_SGTTY)
	endif(NOT DEFINED HAVE_TERMIOS)

	IF(HAVE_TERMIOS)
		ADD_TCL_CFLAG(USE_TERMIOS)
	ELSE(HAVE_TERMIOS)
		IF(HAVE_TERMIO)
		   ADD_TCL_CFLAG(USE_TERMIO)
		ELSE(HAVE_TERMIO)
			IF(HAVE_SGTTY)
				ADD_TCL_CFLAG(USE_SGTTY)
			ENDIF(HAVE_SGTTY)
		ENDIF(HAVE_TERMIO)
	ENDIF(HAVE_TERMIOS)
ENDMACRO(SC_SERIAL_PORT)


#--------------------------------------------------------------------
# SC_MISSING_POSIX_HEADERS
#--------------------------------------------------------------------
MACRO(SC_MISSING_POSIX_HEADERS)
	TCL_CHECK_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
	IF(NOT HAVE_DIRENT_H)
		add_definitions(-DNO_DIRENT_H=1)
	ENDIF(NOT HAVE_DIRENT_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(float.h FLOAT_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(values.h VALUES_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(limits.h LIMITS_H)
	TCL_CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
	TCL_CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
	IF(NOT HAVE_STRING_H)
		add_definitions(-DNO_STRING_H=1)
	ENDIF(NOT HAVE_STRING_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(sys/wait.h SYS_WAIT_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(dlfcn.h DLFCN_H)
	TCL_CHECK_INCLUDE_FILE_USABILITY(sys/param.h SYS_PARAM_H)
ENDMACRO(SC_MISSING_POSIX_HEADERS)


#--------------------------------------------------------------------
# SC_PATH_X
#--------------------------------------------------------------------

# Replaced by CMake's FindX11

#--------------------------------------------------------------------
# SC_BLOCKING_STYLE
#--------------------------------------------------------------------
TCL_CHECK_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H) 
TCL_CHECK_INCLUDE_FILE(sys/filio.h HAVE_SYS_FILIO_H)
IF(${CMAKE_SYSTEM_NAME} MATCHES "^OSF.*")
	ADD_TCL_CFLAG(USE_FIONBIO)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^OSF.*")
IF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
	STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
	IF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 5)
		ADD_TCL_CFLAG(USE_FIONBIO)
	ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 5)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")

#--------------------------------------------------------------------
# SC_TIME_HANLDER
#
# The TEA version of this macro calls AC_HEADER_TIME, but Autotools
# docs list it as obsolete.
#
# TODO - tzname testing from AC_STRUCT_TIMEZONE is incomplete
#
#--------------------------------------------------------------------
MACRO(SC_TIME_HANDLER)
	TCL_CHECK_INCLUDE_FILE_USABILITY(sys/time.h SYS_TIME_H)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_zone time.h STRUCT_TM_TM_ZONE)
	IF(HAVE_STRUCT_TM_TM_ZONE)
		ADD_TCL_CFLAG(HAVE_TM_ZONE)
	ELSE(HAVE_STRUCT_TM_TM_ZONE)
		SET(TZNAME_SRC "
#include <time.h>
int main () {
#ifndef tzname
  (void) tzname;
#endif
return 0;
}")
      CHECK_C_SOURCE_COMPILES("${TZNAME_SRC}" HAVE_TZNAME)
		IF(HAVE_TZNAME)
			ADD_TCL_CFLAG(HAVE_DECL_TZNAME)
		ENDIF(HAVE_TZNAME)
	ENDIF(HAVE_STRUCT_TM_TM_ZONE)
	TCL_CHECK_FUNCTION_EXISTS(gmtime_r HAVE_GMTIME_R)
	TCL_CHECK_FUNCTION_EXISTS(localtime_r HAVE_LOCALTIME_R)
	TCL_CHECK_FUNCTION_EXISTS(mktime HAVE_MKTIME)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_tzadj time.h TM_TZADJ)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_gmtoff time.h TM_GMTOFF)
	SET(TZONE_SRC_1 "
#include <time.h>
int main () {
extern long timezone;
timezone += 1;
exit (0);
return 0;
}
   ")
	CHECK_C_SOURCE_COMPILES("${TZONE_SRC_1}" HAVE_TIMEZONE_VAR)
   IF(HAVE_TIMEZONE_VAR)
		ADD_TCL_CFLAG(HAVE_TIMEZONE_VAR)
	ELSE(HAVE_TIMEZONE_VAR)
		SET(TZONE_SRC_2 "
#include <time.h>
int main() {
extern time_t timezone;
timezone += 1;
exit (0);
return 0;
}
      ")
		CHECK_C_SOURCE_COMPILES("${TZONE_SRC_2}" HAVE_TIMEZONE_VAR)
		IF(HAVE_TIMEZONE_VAR)
			ADD_TCL_CFLAG(HAVE_TIMEZONE_VAR)
		ENDIF(HAVE_TIMEZONE_VAR)
	ENDIF(HAVE_TIMEZONE_VAR)
	IF(HAVE_SYS_TIME_H)
		SET(TIME_WITH_SYS_TIME_SRC "
#include <sys/time.h>
#include <time.h>
int main() {
extern time_t timezone;
timezone += 1;
exit (0);
return 0;
}
      ")
		CHECK_C_SOURCE_COMPILES("${TIME_WITH_SYS_TIME_SRC}" TIME_WITH_SYS_TIME_WORKS)
		IF(TIME_WITH_SYS_TIME_WORKS)
			add_definitions(-DTIME_WITH_SYS_TIME=1)
		ENDIF(TIME_WITH_SYS_TIME_WORKS)
	ENDIF(HAVE_SYS_TIME_H)
ENDMACRO(SC_TIME_HANDLER)

#--------------------------------------------------------------------
# SC_BUGGY_STRTOD
#--------------------------------------------------------------------

# TODO


#--------------------------------------------------------------------
# SC_TCL_LINK_LIBS
#--------------------------------------------------------------------
MACRO(SC_TCL_LINK_LIBS)
	SET(TCL_LINK_LIBS "")

	# Math libraries 
	IF(NOT WIN32)
		CHECK_LIBRARY_EXISTS(m cos "" HAVE_M_LIBRARY)
		IF(HAVE_M_LIBRARY)
			set(M_LIBRARY "m")
		ENDIF(HAVE_M_LIBRARY)
		#find_library(IEEE_LIB ieee)
		#MARK_AS_ADVANCED(IEEE_LIB)
		#IF(IEEE_LIB)
		#	SET(M_LIBRARY "-lieee ${M_LIBRARY}")
		#ENDIF(IEEE_LIB)
		CHECK_LIBRARY_EXISTS(sunmath ieee_flags "" HAVE_SUNMATH_LIBRARY)
		IF(HAVE_SUNMATH_LIBRARY)
			SET(M_LIBRARY "-lsunmath ${M_LIBRARY}")
		ENDIF(HAVE_SUNMATH_LIBRARY)
	ENDIF(NOT WIN32)

	IF(NOT INET_LIBRARY AND NOT INET_QUIET)
		MESSAGE("-- Looking for INET library")
		set(INET_LIBRARY "inet")
		IF(INET_LIBRARY)
			MESSAGE("-- Found INET library: ${INET_LIBRARY}")
		ELSE(INET_LIBRARY)
			MESSAGE("-- Looking for INET library - not found")
		ENDIF(INET_LIBRARY)
		SET(INET_QUIET 1 CACHE STRING "INET quiet")
		MARK_AS_ADVANCED(INET_QUIET)
	ENDIF(NOT INET_LIBRARY AND NOT INET_QUIET)
	IF(TCL_LINK_LIBS)
		SET(TCL_LINK_LIBS "${TCL_LINK_LIBS};${M_LIBRARY};${INET_LIBRARY}")
	ELSEIF(TCL_LINK_LIBS)
		SET(TCL_LINK_LIBS "${M_LIBRARY};${INET_LIBRARY}")
	ENDIF(TCL_LINK_LIBS)
	MARK_AS_ADVANCED(TCL_LINK_LIBS)

	TCL_CHECK_INCLUDE_FILE_USABILITY(net/errno.h NET_ERRNO_H)
	CHECK_FUNCTION_EXISTS(connect HAVE_CONNECT)
	IF(NOT HAVE_CONNECT)
		CHECK_FUNCTION_EXISTS(setsockopt HAVE_SETSOCKOPT)
		IF(NOT HAVE_SETSOCKOPT)
			CHECK_LIBRARY_EXISTS(socket connect "" HAVE_SOCKET_LIBRARY)
			IF(HAVE_SOCKET_LIBRARY)
				set(SOCKET_LIBRARY "socket")
				IF(TCL_LINK_LIBS)
					SET(TCL_LINK_LIBS ${TCL_LINK_LIBS};${SOCKET_LIBRARY})
				ELSE(TCL_LINK_LIBS)
					SET(TCL_LINK_LIBS ${SOCKET_LIBRARY})
				ENDIF(TCL_LINK_LIBS)
			ENDIF(HAVE_SOCKET_LIBRARY)
		ENDIF(NOT HAVE_SETSOCKOPT)
	ENDIF(NOT HAVE_CONNECT)
	CHECK_FUNCTION_EXISTS(gethostbyname HAVE_GETHOSTBYNAME)
	IF(NOT HAVE_GETHOSTBYNAME)
		CHECK_LIBRARY_EXISTS(nsl gethostbyname "" HAVE_NSL_LIBRARY)
		IF(HAVE_NSL_LIBRARY)
			set(NSL_LIBRARY "nsl")
			IF(TCL_LINK_LIBS)
				SET(TCL_LINK_LIBS ${TCL_LINK_LIBS};${NSL_LIBRARY})
			ELSE(TCL_LINK_LIBS)
				SET(TCL_LINK_LIBS ${NSL_LIBRARY})
			ENDIF(TCL_LINK_LIBS)
		ENDIF(HAVE_NSL_LIBRARY)
	ENDIF(NOT HAVE_GETHOSTBYNAME)

ENDMACRO(SC_TCL_LINK_LIBS)

#--------------------------------------------------------------------
# SC_TCL_EARLY_FLAGS
#--------------------------------------------------------------------

# TODO - needed at all for CMake?

#--------------------------------------------------------------------
# SC_TCL_64BIT_FLAGS
#
# Detect and set up 64-bit compiling here.  LOTS of TODO here
#--------------------------------------------------------------------
MACRO(SC_TCL_64BIT_FLAGS)
	IF(NOT CMAKE_SIZEOF_VOID_P)
		MESSAGE(WARNING "CMAKE_SIZEOF_VOID_P is not defined - assuming 32-bit platform")
		SET(CMAKE_SIZEOF_VOID_P 4)
	ENDIF(NOT CMAKE_SIZEOF_VOID_P)
	IF(NOT CMAKE_WORD_SIZE)
		IF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
			SET(CMAKE_WORD_SIZE "64BIT")
		ELSE(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
			SET(CMAKE_WORD_SIZE "32BIT")
		ENDIF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
	ENDIF(NOT CMAKE_WORD_SIZE)
	IF(NOT TCL_ENABLE_64BIT)
		IF(${CMAKE_WORD_SIZE} MATCHES "64BIT")
			set(TCL_ENABLE_64BIT "ON" CACHE STRING "Enable 64-bit support")
		ELSE(${CMAKE_WORD_SIZE} MATCHES "64BIT")
			set(TCL_ENABLE_64BIT "OFF" CACHE STRING "Enable 64-bit support")
		ENDIF(${CMAKE_WORD_SIZE} MATCHES "64BIT")
	ENDIF(NOT TCL_ENABLE_64BIT)
	IF(NOT ${TCL_ENABLE_64BIT} MATCHES "OFF")
		IF(${CMAKE_WORD_SIZE} MATCHES "64BIT")
			set(TCL_ENABLE_64BIT ON CACHE STRING "Enable 64-bit support")
		ELSE(${CMAKE_WORD_SIZE} MATCHES "64BIT")
			set(TCL_ENABLE_64BIT OFF CACHE STRING "Enable 64-bit support")
		ENDIF(${CMAKE_WORD_SIZE} MATCHES "64BIT")
	ENDIF(NOT ${TCL_ENABLE_64BIT} MATCHES "OFF")
	set(TCL_ENABLE_64BIT ${TCL_ENABLE_64BIT} CACHE STRING "Enable 64-bit support")
	IF(TCL_ENABLE_64BIT)
		IF(NOT 64BIT_FLAG)
			CHECK_C_FLAG("arch x86_64" 64BIT_FLAG)
			CHECK_C_FLAG(64 64BIT_FLAG)
			CHECK_C_FLAG("mabi=64" 64BIT_FLAG)
			CHECK_C_FLAG(m64 64BIT_FLAG)
			CHECK_C_FLAG(q64 64BIT_FLAG)
		ENDIF(NOT 64BIT_FLAG)
		MARK_AS_ADVANCED(64BIT_FLAG)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${64BIT_FLAG}")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${64BIT_FLAG}")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${64BIT_FLAG}")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${64BIT_FLAG}")
		add_definitions(-DTCL_CFG_DO64BIT=1)
	ENDIF(TCL_ENABLE_64BIT)
	IF(CMAKE_CL_64)
		add_definitions(-D_stati64=_stat64)
	ENDIF(CMAKE_CL_64)
ENDMACRO(SC_TCL_64BIT_FLAGS)

#--------------------------------------------------------------------
# SC_TCL_CFG_ENCODING   TIP #59
#--------------------------------------------------------------------
MACRO(SC_TCL_CFG_ENCODING)
	IF(NOT TCL_CFGVAL_ENCODING)
		IF(WIN32)
			SET(TCL_CFGVAL_ENCODING "cp1252")
		ELSE(WIN32)
			SET(TCL_CFGVAL_ENCODING "iso8859-1")
		ENDIF(WIN32)
	ENDIF(NOT TCL_CFGVAL_ENCODING)
	add_definitions(-DTCL_CFGVAL_ENCODING="${TCL_CFGVAL_ENCODING}")
ENDMACRO(SC_TCL_CFG_ENCODING)


#--------------------------------------------------------------------
# SC_TCL_CHECK_BROKEN_FUNC
#--------------------------------------------------------------------
MACRO(SC_TCL_CHECK_BROKEN_FUNC)
	CHECK_FUNCTION_EXISTS(${ARGV0} HAVE_${ARGV0})
	IF(HAVE_${ARGV0})
		SET(COMPILE_SRC "
		int main() {
		${ARGV1}
		}")
		if(NOT DEFINED WORKING_${ARGV0})
			CHECK_C_SOURCE_RUNS("${COMPILE_SRC}"  WORKING_${ARGV0})
		endif(NOT DEFINED WORKING_${ARGV0})
		IF(NOT WORKING_${ARGV0})
			SET(COMPAT_SRCS ${COMPAT_SRCS} compat/${ARGV0}.c CACHE STRING "Compatibility srcs" FORCE)
		ENDIF(NOT WORKING_${ARGV0})
	ELSE(HAVE_${ARGV0})
		SET(COMPAT_SRCS ${COMPAT_SRCS} compat/${ARGV0}.c CACHE STRING "Compatibility srcs" FORCE)
	ENDIF(HAVE_${ARGV0})
ENDMACRO(SC_TCL_CHECK_BROKEN_FUNC)

#--------------------------------------------------------------------
# SC_TCL_GETHOSTBYADDR_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETHOSTBYADDR_R)
	TCL_CHECK_FUNCTION_EXISTS(gethostbyaddr_r HAVE_GETHOSTBYADDR_R)
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
			ADD_TCL_CFLAG(HAVE_GETHOSTBYADDR_R_7)
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
			   ADD_TCL_CFLAG(HAVE_GETHOSTBYADDR_R_8)
			ENDIF(HAVE_GETHOSTBYADDR_R_8)
		ENDIF(HAVE_GETHOSTBYADDR_R_7)
	ENDIF(HAVE_GETHOSTBYADDR_R)
ENDMACRO(SC_TCL_GETHOSTBYADDR_R)

#--------------------------------------------------------------------
# SC_TCL_GETHOSTBYNAME_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETHOSTBYNAME_R)
	TCL_CHECK_FUNCTION_EXISTS(gethostbyname_r HAVE_GETHOSTBYNAME_R)
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
		CHECK_C_SOURCE_COMPILES("${HAVE_GETHOSTBYNAME_R_6_SRC}"  HAVE_GETHOSTBYNAME_R_6)
		IF(HAVE_GETHOSTBYNAME_R_6)
			ADD_TCL_CFLAG(HAVE_GETHOSTBYNAME_R_6)
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
				ADD_TCL_CFLAG(HAVE_GETHOSTBYNAME_R_5)
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
					ADD_TCL_CFLAG(HAVE_GETHOSTBYNAME_R_3)
				ENDIF(HAVE_GETHOSTBYNAME_R_3)
			ENDIF(HAVE_GETHOSTBYNAME_R_5)
		ENDIF(HAVE_GETHOSTBYNAME_R_6)
	ENDIF(HAVE_GETHOSTBYNAME_R)
ENDMACRO(SC_TCL_GETHOSTBYNAME_R)

#--------------------------------------------------------------------
# SC_TCL_IPV6
#--------------------------------------------------------------------
MACRO(SC_TCL_IPV6)
	CHECK_FUNCTION_EXISTS(getaddrinfo HAVE_GETADDRINFO)
	IF(HAVE_GETADDRINFO)
		SET(GETADDRINFO_SRC "
#include <netdb.h>
int main () {
const char *name, *port;
struct addrinfo *aiPtr, hints;
(void)getaddrinfo(name,port, &hints, &aiPtr);
(void)freeaddrinfo(aiPtr);
return 0;
}
		")
		CHECK_C_SOURCE_COMPILES("${GETADDRINFO_SRC}" WORKING_GETADDRINFO)
		IF(WORKING_GETADDRINFO)
			ADD_TCL_CFLAG(HAVE_GETADDRINFO)
		ENDIF(WORKING_GETADDRINFO)
	ENDIF(HAVE_GETADDRINFO)
ENDMACRO(SC_TCL_IPV6)

#--------------------------------------------------------------------
# SC_TCL_GETPWUID_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETPWUID_R)
	TCL_CHECK_FUNCTION_EXISTS(getpwuid_r HAVE_GETPWUID_R)
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
			ADD_TCL_CFLAG(HAVE_GETPWUID_R_5)
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
				ADD_TCL_CFLAG(HAVE_GETPWUID_R_4)
			ENDIF(HAVE_GETPWUID_R_4)
		ENDIF(HAVE_GETPWUID_R_5)
	ENDIF(HAVE_GETPWUID_R)
ENDMACRO(SC_TCL_GETPWUID_R)

#--------------------------------------------------------------------
# SC_TCL_GETPWNAM_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETPWNAM_R)
	TCL_CHECK_FUNCTION_EXISTS(getpwnam_r HAVE_GETPWNAM_R)
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
			ADD_TCL_CFLAG(HAVE_GETPWNAM_R_5)
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
			   ADD_TCL_CFLAG(HAVE_GETPWNAM_R_4)
			ENDIF(HAVE_GETPWNAM_R_4)
		ENDIF(HAVE_GETPWNAM_R_5)
	ENDIF(HAVE_GETPWNAM_R)
ENDMACRO(SC_TCL_GETPWNAM_R)

#--------------------------------------------------------------------
# SC_TCL_GETGRGID_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETGRGID_R)
	TCL_CHECK_FUNCTION_EXISTS(getgrgid_r HAVE_GETGRGID_R)
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
			ADD_TCL_CFLAG(HAVE_GETGRGID_R_5)
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
				ADD_TCL_CFLAG(HAVE_GETGRGID_R_4)
			ENDIF(HAVE_GETGRGID_R_4)
		ENDIF(HAVE_GETGRGID_R_5)
	ENDIF(HAVE_GETGRGID_R)
ENDMACRO(SC_TCL_GETGRGID_R)

#--------------------------------------------------------------------
# SC_TCL_GETGRNAM_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETGRNAM_R)
	TCL_CHECK_FUNCTION_EXISTS(getgrnam_r HAVE_GETGRNAM_R)
	IF(HAVE_GETGRNAM_R)
		SET(HAVE_GETGRNAM_R_5_SRC "
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
		CHECK_C_SOURCE_COMPILES("${HAVE_GETGRNAM_R_5_SRC}"  HAVE_GETGRNAM_R_5)
		IF(HAVE_GETGRNAM_R_5)
			ADD_TCL_CFLAG(HAVE_GETGRNAM_R_5)
		ELSE(HAVE_GETGRNAM_R_5)
			SET(HAVE_GETGRNAM_R_4_SRC "
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
			CHECK_C_SOURCE_COMPILES("${HAVE_GETGRNAM_R_4_SRC}" HAVE_GETGRNAM_R_4)
			IF(HAVE_GETGRNAM_R_4)
			   ADD_TCL_CFLAG(HAVE_GETGRNAM_R_4)
			ENDIF(HAVE_GETGRNAM_R_4)
		ENDIF(HAVE_GETGRNAM_R_5)
	ENDIF(HAVE_GETGRNAM_R)
ENDMACRO(SC_TCL_GETGRNAM_R)




MACRO(CHECK_FD_SET_IN_TYPES_D)
	SET(TEST_SRC "
	#include <sys/types.h>
	int main ()
	{
	fd_set readMask, writeMask;
	return 0;
	}
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" FD_SET_IN_TYPES_H)
ENDMACRO(CHECK_FD_SET_IN_TYPES_D)

MACRO(CHECK_COMPILER_SUPPORTS_HIDDEN_D)
	SET(TEST_SRC "
	#define MODULE_SCOPE extern __attribute__((__visibility__(\"hidden\")))
	main(){};
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" COMPILER_SUPPORTS_HIDDEN)
ENDMACRO(CHECK_COMPILER_SUPPORTS_HIDDEN_D)



