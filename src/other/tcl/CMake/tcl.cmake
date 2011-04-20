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
INCLUDE(ResolveCompilerPaths)
INCLUDE(CheckPrototypeExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCFileRuns)

INCLUDE(ac_std_funcs)

MACRO(ADD_TCL_CFLAG TCL_CFLAG)
	SET(TCL_CFLAGS "${TCL_CFLAGS} -D${TCL_CFLAG}=1" CACHE STRING "TCL CFLAGS" FORCE)
ENDMACRO(ADD_TCL_CFLAG)

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
	OPTION(TCL_THREADS "Enable Tcl Thread support" OFF)
	IF(TCL_THREADS)
		ADD_TCL_CFLAG(TCL_THREADS)
		ADD_TCL_CFLAG(USE_THREAD_ALLOC)
		ADD_TCL_CFLAG(_REENTRANT)
		ADD_TCL_CFLAG(_THREAD_SAFE)
		IF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
			ADD_TCL_CFLAG(_POSIX_PTHREAD_SEMANTICS)
		ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^SunOS$")
		FIND_PACKAGE(Threads)
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
	OPTION(ENABLE_LANGINFO "Trigger use of nl_langinfo if available." ON)
	IF(ENABLE_LANGINFO)
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
				SET(ENABLE_LANGINFO OFF CACHE STRING "Langinfo off" FORCE)
			ENDIF(LANGINFO_COMPILES)
		ELSE(HAVE_LANGINFO)
			SET(ENABLE_LANGINFO OFF CACHE STRING "Langinfo off" FORCE)
		ENDIF(HAVE_LANGINFO)
		MARK_AS_ADVANCED(ENABLE_LANGINFO)
	ENDIF(ENABLE_LANGINFO)
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
	CONFIG_CHECK_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
	IF(NOT HAVE_DIRENT_H)
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_DIRENT_H=1")
	ENDIF(NOT HAVE_DIRENT_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(float.h FLOAT_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(values.h VALUES_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(limits.h LIMITS_H)
	CONFIG_CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
	CONFIG_CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
	IF(NOT HAVE_STRING_H)
		SET(TCL_CFLAGS "${TCL_CFLAGS} -DNO_STRING_H=1")
	ENDIF(NOT HAVE_STRING_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(sys/wait.h SYS_WAIT_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(dlfcn.h DLFCN_H)
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(sys/param.h SYS_PARAM_H)
ENDMACRO(SC_MISSING_POSIX_HEADERS)


#--------------------------------------------------------------------
# SC_PATH_X
#--------------------------------------------------------------------

# Replaced by CMake's FindX11

#--------------------------------------------------------------------
# SC_BLOCKING_STYLE
#--------------------------------------------------------------------
CONFIG_CHECK_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H) 
CONFIG_CHECK_INCLUDE_FILE(sys/filio.h HAVE_SYS_FILIO_H)
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
	CONFIG_CHECK_INCLUDE_FILE_USABILITY(sys/time.h SYS_TIME_H)
	CONFIG_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_zone time.h STRUCT_TM_TM_ZONE)
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
	CONFIG_CHECK_FUNCTION_EXISTS(gmtime_r HAVE_GMTIME_R)
	CONFIG_CHECK_FUNCTION_EXISTS(localtime_r HAVE_LOCALTIME_R)
	CONFIG_CHECK_FUNCTION_EXISTS(mktime HAVE_MKTIME)
	CONFIG_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_tzadj time.h TM_TZADJ)
	CONFIG_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_gmtoff time.h TM_GMTOFF)
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
	CHECK_FUNCTION_EXISTS(sin, HAVE_MATHLIB)
	IF(NOT HAVE_MATHLIB)
		CONFIG_CHECK_LIBRARY(M m sin)
	ENDIF(NOT HAVE_MATHLIB)
	IF(NOT IEEE_LIBRARY AND NOT IEEE_QUIET)
		MESSAGE("-- Looking for IEEE library")
		RESOLVE_LIBRARIES(IEEE_LIBRARY "-lieee")
		IF(IEEE_LIBRARY)
			MESSAGE("-- Found IEEE library: ${IEEE_LIBRARY}")
		ELSE(IEEE_LIBRARY)
			MESSAGE("-- Looking for IEEE library - not found")
		ENDIF(IEEE_LIBRARY)
		SET(IEEE_QUIET 1 CACHE STRING "IEEE quiet")
		MARK_AS_ADVANCED(IEEE_QUIET)
	ENDIF(NOT IEEE_LIBRARY AND NOT IEEE_QUIET)
	IF(NOT INET_LIBRARY AND NOT INET_QUIET)
		MESSAGE("-- Looking for INET library")
		RESOLVE_LIBRARIES(INET_LIBRARY "-linet")
		IF(INET_LIBRARY)
			MESSAGE("-- Found INET library: ${INET_LIBRARY}")
		ELSE(INET_LIBRARY)
			MESSAGE("-- Looking for INET library - not found")
		ENDIF(INET_LIBRARY)
		SET(INET_QUIET 1 CACHE STRING "INET quiet")
		MARK_AS_ADVANCED(INET_QUIET)
	ENDIF(NOT INET_LIBRARY AND NOT INET_QUIET)

	SET(TCL_LINK_LIBS ${TCL_LINK_LIBS} ${IEEE_LIBRARY} ${M_LIBRARY} ${INET_LIBRARY} CACHE STRING "TCL CFLAGS" FORCE)
	MARK_AS_ADVANCED(TCL_LINK_LIBS)

	CONFIG_CHECK_INCLUDE_FILE_USABILITY(net/errno.h NET_ERRNO_H)
	CHECK_FUNCTION_EXISTS(connect HAVE_CONNECT)
	IF(NOT HAVE_CONNECT)
		CHECK_FUNCTION_EXISTS(setsockopt HAVE_SETSOCKOPT)
		IF(NOT HAVE_SETSOCKOPT)
			CONFIG_CHECK_LIBRARY(SOCKET socket setsocket)
			SET(TCL_LINK_LIBS ${TCL_LINK_LIBS} ${SOCKET_LIBRARY} CACHE STRING "TCL CFLAGS" FORCE)
		ENDIF(NOT HAVE_SETSOCKOPT)
	ENDIF(NOT HAVE_CONNECT)
	CHECK_FUNCTION_EXISTS(gethostbyname HAVE_GETHOSTBYNAME)
	IF(NOT HAVE_GETHOSTBYNAME)
		CONFIG_CHECK_LIBRARY(NLS nls gethostbyname)
		SET(TCL_LINK_LIBS ${TCL_LINK_LIBS} ${NLS_LIBRARY} CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(NOT HAVE_GETHOSTBYNAME)

ENDMACRO(SC_TCL_LINK_LIBS)

#--------------------------------------------------------------------
# SC_TCL_EARLY_FLAGS
#--------------------------------------------------------------------

# TODO - needed at all for CMake?

#--------------------------------------------------------------------
# SC_TCL_64BIT_FLAGS
#
# Detect and set up 64 bit compiling here.  LOTS of TODO here
#--------------------------------------------------------------------
MACRO(SC_TCL_64BIT_FLAGS)
	OPTION(ENABLE_64_BIT "64 bit void pointer" ON)
	IF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$" AND ENABLE_64_BIT)
		IF(ENABLE_64_BIT)
			IF(NOT 64BIT_FLAG)
				MESSAGE("Checking for 64-bit support:")
				CHECK_C_FLAG("arch x86_64" 64BIT_FLAG)
				CHECK_C_FLAG(64 64BIT_FLAG)
				CHECK_C_FLAG("mabi=64" 64BIT_FLAG)
				CHECK_C_FLAG(m64 64BIT_FLAG)
				CHECK_C_FLAG(q64 64BIT_FLAG)
			ENDIF(NOT 64BIT_FLAG)
			SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${64BIT_FLAG}")
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${64BIT_FLAG}")
		ENDIF(ENABLE_64_BIT)
	ELSE(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$" AND ENABLE_64_BIT)
		SET(ENABLE_64_BIT OFF)
	ENDIF(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$" AND ENABLE_64_BIT)
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
	SET(TCL_CFLAGS "${TCL_CFLAGS} -DTCL_CFGVAL_ENCODING=\\\"${TCL_CFGVAL_ENCODING}\\\"" CACHE STRING "TCL CFLAGS" FORCE)
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
		CHECK_C_SOURCE_RUNS("${COMPILE_SRC}"  WORKING_${ARGV0})
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
	CONFIG_CHECK_FUNCTION_EXISTS(gethostbyaddr_r HAVE_GETHOSTBYADDR_R)
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
	CONFIG_CHECK_FUNCTION_EXISTS(gethostbyname_r HAVE_GETHOSTBYNAME_R)
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
# SC_TCL_GETADDRINFO
#--------------------------------------------------------------------
MACRO(SC_TCL_GETADDRINFO)
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
ENDMACRO(SC_TCL_GETADDRINFO)

#--------------------------------------------------------------------
# SC_TCL_GETPWUID_R
#--------------------------------------------------------------------
MACRO(SC_TCL_GETPWUID_R)
	CONFIG_CHECK_FUNCTION_EXISTS(getpwuid_r HAVE_GETPWUID_R)
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
	CONFIG_CHECK_FUNCTION_EXISTS(getpwnam_r HAVE_GETPWNAM_R)
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
	CONFIG_CHECK_FUNCTION_EXISTS(getgrgid_r HAVE_GETGRGID_R)
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
	CONFIG_CHECK_FUNCTION_EXISTS(getgrnam_r HAVE_GETGRNAM_R)
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
	SET(TEST_SRC"
	#define MODULE_SCOPE extern __attribute__((__visibility__("hidden")))
	main(){};
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" COMPILER_SUPPORTS_HIDDEN)
ENDMACRO(CHECK_COMPILER_SUPPORTS_HIDDEN_D)



