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
	CHECK_INCLUDE_FILE(${filename} ${var})
	IF(${var})
		SET(HEADER_SRC "
#include <${filename}>
main(){};
		")
		CHECK_C_SOURCE_COMPILES("${HEADER_SRC}" ${var}_USABLE)
	ENDIF(${var})
	IF(${var} OR ${var}_USABLE)
		add_definitions(-D${var}=1)
	ENDIF(${var} OR ${var}_USABLE)
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
   CHECK_TYPE_SIZE(${typename} ${var}_T)
   SET(CHECK_EXTRA_INCLUDE_FILES)
   IF(${var}_T)
      add_definitions(-D${var}_T=1)
   ENDIF(${var}_T)
ENDMACRO(TCL_CHECK_TYPE_SIZE)

# Check for a member of a structure
MACRO(TCL_CHECK_STRUCT_HAS_MEMBER structname member header var)
   CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} ${var})
   IF(${var})
      add_definitions(-D${var}=1)
   ENDIF(${var})
ENDMACRO(TCL_CHECK_STRUCT_HAS_MEMBER)

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
	TCL_CHECK_INCLUDE_FILE_USABILITY(sys/time.h HAVE_SYS_TIME_H)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_zone time.h HAVE_STRUCT_TM_TM_ZONE)
	IF(HAVE_STRUCT_TM_TM_ZONE)
		add_definitions(-DHAVE_TM_ZONE=1)
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
			add_definitions(-DHAVE_DECL_TZNAME=1)
		ENDIF(HAVE_TZNAME)
	ENDIF(HAVE_STRUCT_TM_TM_ZONE)
	TCL_CHECK_FUNCTION_EXISTS(gmtime_r HAVE_GMTIME_R)
	TCL_CHECK_FUNCTION_EXISTS(localtime_r HAVE_LOCALTIME_R)
	TCL_CHECK_FUNCTION_EXISTS(mktime HAVE_MKTIME)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_tzadj time.h HAVE_TM_TZADJ)
	TCL_CHECK_STRUCT_HAS_MEMBER("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)
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
		add_definitions(-DHAVE_TIMEZONE_VAR=1)
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
			add_definitions(-DHAVE_TIMEZONE_VAR=1)
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

	TCL_CHECK_INCLUDE_FILE_USABILITY(net/errno.h HAVE_NET_ERRNO_H)
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
# SC_TCL_64BIT_FLAGS
#
# Detect and set up 64-bit compiling here.  LOTS of TODO here
#--------------------------------------------------------------------
MACRO(SC_TCL_64BIT_FLAGS)
	# See if we should use long anyway.  Note that we substitute
	# in the type that is our current guess (long long) for a
	# 64-bit type inside this check program.
	SET(LONG_SRC "
int main () {
    switch (0) {
        case 1: case (sizeof(long long)==sizeof(long)): ;
    }
return 0;
}
   ")
	CHECK_C_SOURCE_COMPILES("${LONG_SRC}" LONG_NOT_LONG_LONG)
	IF(NOT LONG_NOT_LONG_LONG)
		add_definitions(-DTCL_WIDE_INT_IS_LONG=1)
	ENDIF(NOT LONG_NOT_LONG_LONG)

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
			add_definitions(-DHAVE_GETHOSTBYADDR_R_7=1)
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
			   add_definitions(-DHAVE_GETHOSTBYADDR_R_8=1)
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
			add_definitions(-DHAVE_GETHOSTBYNAME_R_6=1)
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
				add_definitions(-DHAVE_GETHOSTBYNAME_R_5=1)
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
					add_definitions(-DHAVE_GETHOSTBYNAME_R_3=1)
				ENDIF(HAVE_GETHOSTBYNAME_R_3)
			ENDIF(HAVE_GETHOSTBYNAME_R_5)
		ENDIF(HAVE_GETHOSTBYNAME_R_6)
	ENDIF(HAVE_GETHOSTBYNAME_R)
ENDMACRO(SC_TCL_GETHOSTBYNAME_R)

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
			add_definitions(-DHAVE_GETPWUID_R_5=1)
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
				add_definitions(-DHAVE_GETPWUID_R_4=1)
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
			add_definitions(-DHAVE_GETPWNAM_R_5=1)
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
			   add_definitions(-DHAVE_GETPWNAM_R_4=1)
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
			add_definitions(-DHAVE_GETGRGID_R_5=1)
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
				add_definitions(-DHAVE_GETGRGID_R_4=1)
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
			add_definitions(-DHAVE_GETGRNAM_R_5=1)
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
			   add_definitions(-DHAVE_GETGRNAM_R_4=1)
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



