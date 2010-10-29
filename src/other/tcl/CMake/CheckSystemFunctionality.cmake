# Checking compiler flags benefits from some macro logic

INCLUDE(CheckCCompilerFlag)

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
				MESSAGE("Found ${ARGV1} - setting to -${flag}")
				SET(${ARGV1} "-${flag}" CACHE STRING "${ARGV1}" FORCE)
			ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
		ENDIF(NOT ${ARGV1})
	ENDIF(${ARGC} LESS 2)
	IF(${UPPER_FLAG}_COMPILER_FLAG)
		SET(${UPPER_FLAG}_COMPILER_FLAG "-${flag}")
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

MACRO(CHECK_C_FLAG_GATHER flag FLAGS)
	STRING(TOUPPER ${flag} UPPER_FLAG)
	STRING(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
	CHECK_C_COMPILER_FLAG(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
	IF(${UPPER_FLAG}_COMPILER_FLAG)
		SET(${FLAGS} "${${FLAGS}} -${flag}")
	ENDIF(${UPPER_FLAG}_COMPILER_FLAG)
ENDMACRO()

# CMake does not by default include functionality similar to autoheader in 
# GNU Autotools, but is it not difficult to reproduce.  There are two aspects
# to these tests - the first being the generation of a header file defining
# variables, and the second being the running of various custom tests to establish
# whether features actually work on a given platform.  The first step is handled
# by creating wrapper macros for the standard CMake functions that also write
# to a CONFIG_H_FILE if it is defined, with a couple extra conveniences.  The second
# and more labor intensive step is to create tests using the CMake functions
# that reproduce the functionality tests expected by source code using variables
# defined by Autotools tests.  This file does not have all such tests, for each project 
# has its own requirements and many thousands of these tests can be devised.  The macro
# collection below covers the requirements of the projects that have thus far been
# converted to use this particular compilation mechanism, and will be added to as
# needed.


# Automate putting variables from tests into a config.h.in file,
# and otherwise wrap check macros in extra logic as needed

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCSourceCompiles)
INCLUDE(ResolveCompilerPaths)

MACRO(CHECK_FUNCTION_EXISTS_D function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(${var})
			 SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -D${var}=1" CACHE STRING "TCL CFLAGS" FORCE)
  endif(${var})
ENDMACRO(CHECK_FUNCTION_EXISTS_D)

MACRO(CHECK_INCLUDE_FILE_D filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  if(${var})
			 SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -D${var}=1" CACHE STRING "TCL CFLAGS" FORCE)
  endif(${var})
ENDMACRO(CHECK_INCLUDE_FILE_D)

MACRO(CHECK_INCLUDE_FILE_USABILITY_D filename var)
	CHECK_INCLUDE_FILE_D(${filename} HAVE_${var})
	IF(HAVE_${var})
		SET(HEADER_SRC "
		#include <${filename}>
		main(){};
		")
		CHECK_C_SOURCE_COMPILES("${HEADER_SRC}" ${var}_USABLE)
	ENDIF(HAVE_${var})
	IF(NOT HAVE_${var} OR NOT ${var}_USABLE)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DNO_${var}=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(NOT HAVE_${var} OR NOT ${var}_USABLE)
ENDMACRO(CHECK_INCLUDE_FILE_USABILITY_D filename var)

MACRO(CHECK_INCLUDE_FILE_CXX_D filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  if(${var})
	 SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -D${var}=1" CACHE STRING "TCL CFLAGS" FORCE)
  endif(${var})
ENDMACRO(CHECK_INCLUDE_FILE_CXX_D)

MACRO(CHECK_TYPE_SIZE_D typename var)
	FOREACH(arg ${ARGN})
		SET(headers ${headers} ${arg})
	ENDFOREACH(arg ${ARGN})
	SET(CHECK_EXTRA_INCLUDE_FILES ${headers})
	CHECK_TYPE_SIZE(${typename} HAVE_${var}_T)
	SET(CHECK_EXTRA_INCLUDE_FILES)
	if(HAVE_${var}_T)
	   SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_${var}_T=1" CACHE STRING "TCL CFLAGS" FORCE)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DSIZEOF_${var}=${HAVE_${var}_T}" CACHE STRING "TCL CFLAGS" FORCE)
	endif(HAVE_${var}_T)
ENDMACRO(CHECK_TYPE_SIZE_D)

MACRO(CHECK_STRUCT_HAS_MEMBER_D structname member header var)
	CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} HAVE_${var})
	if(HAVE_${var})
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_${var}=1" CACHE STRING "TCL CFLAGS" FORCE)
	endif(HAVE_${var})
ENDMACRO(CHECK_STRUCT_HAS_MEMBER_D)

MACRO(CHECK_LIBRARY targetname lname func)
	IF(NOT ${targetname}_LIBRARY)
		CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
		IF(HAVE_${targetname}_${lname})
			RESOLVE_LIBRARIES (${targetname}_LIBRARY "-l${lname}")
			SET(${targetname}_LINKOPT "-l${lname}")
		ENDIF(HAVE_${targetname}_${lname})
	ENDIF(NOT ${targetname}_LIBRARY)
ENDMACRO(CHECK_LIBRARY lname func)

# Functionality testing macros - these also assume a  CONFIG_H file.  In some
# cases the minimal test code can be included in-line, but in others the problem
# of quoting it correctly becomes rather involved.  In those cases, rather than
# complicate the maintainance of the test, the code itself is stored in a file.
# These files need to be present in a directory called test_sources, in one of
# the directories in the CMAKE_MODULE_PATH variable.  The first step is to
# locate such a directory.

SET(CMAKE_TEST_SRCS_DIR "NOTFOUND")
FOREACH($candidate_dir ${CMAKE_MODULE_PATH})
	IF(NOT CMAKE_TEST_SRCS_DIR)
		IF(EXISTS "${candidate_dir}/test_sources" AND IS_DIRECTORY "${candidate_dir}/test_sources")
			SET(CMAKE_TEST_SRCS_DIR ${candidate_dir}/test_sources)
		ENDIF(EXISTS "${candidate_dir}/test_sources" AND IS_DIRECTORY "${candidate_dir}/test_sources")
	ENDIF(NOT CMAKE_TEST_SRCS_DIR)
ENDFOREACH($candidate_dir ${CMAKE_MODULE_PATH})

INCLUDE(CheckPrototypeExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCFileRuns)

MACRO(CHECK_BASENAME_D)
	SET(BASENAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)basename(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${BASENAME_SRC}" HAVE_BASENAME)
	IF(HAVE_BASENAME)
	  SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_BASENAME=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(HAVE_BASENAME)
ENDMACRO(CHECK_BASENAME_D)

MACRO(CHECK_DIRNAME_D)
	SET(DIRNAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)dirname(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${DIRNAME_SRC}" HAVE_DIRNAME)
	IF(HAVE_DIRNAME)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_DIRNAME=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(HAVE_DIRNAME)
ENDMACRO(CHECK_DIRNAME_D)

# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file
MACRO(CMAKE_HEADER_STDC_D)
  CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
  IF(HAVE_STDLIB_H)
  SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_STDLIB_H=1" CACHE STRING "TCL CFLAGS" FORCE)
  ENDIF(HAVE_STDLIB_H)
  CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
  CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
  IF(HAVE_STRING_H)
  SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_STRING_H=1" CACHE STRING "TCL CFLAGS" FORCE)
  ENDIF(HAVE_STRING_H)
  CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
  IF(HAVE_STRINGS_H)
  SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_STRINGS_H=1" CACHE STRING "TCL CFLAGS" FORCE)
  ENDIF(HAVE_STRINGS_H)
  CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
  CHECK_PROTOTYPE_EXISTS(memchr string.h HAVE_STRING_H_MEMCHR)
  CHECK_PROTOTYPE_EXISTS(free stdlib.h HAVE_STDLIB_H_FREE)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
	 SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DSTDC_HEADERS=1" CACHE STRING "TCL CFLAGS" FORCE)
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(CMAKE_HEADER_STDC_D)

# Based on AC_HEADER_SYS_WAIT
MACRO(CMAKE_HEADER_SYS_WAIT_D)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/sys_wait_test.c WORKING_SYS_WAIT)
  IF(WORKING_SYS_WAIT)
	 SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_SYS_WAIT_H=1" CACHE STRING "TCL CFLAGS" FORCE)
  ENDIF(WORKING_SYS_WAIT)
ENDMACRO(CMAKE_HEADER_SYS_WAIT_D)

# Based on AC_FUNC_ALLOCA
MACRO(CMAKE_ALLOCA_D)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_header_test.c WORKING_ALLOCA_H)
	IF(WORKING_ALLOCA_H)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_ALLOCA_H=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(WORKING_ALLOCA_H)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_test.c WORKING_ALLOCA)
	IF(WORKING_ALLOCA)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_ALLOCA=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(WORKING_ALLOCA)
ENDMACRO(CMAKE_ALLOCA_D)

MACRO(CHECK_COMPILER_SUPPORTS_HIDDEN_D)
	SET(TEST_SRC"
	#define MODULE_SCOPE extern __attribute__((__visibility__("hidden")))
	main(){};
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" COMPILER_SUPPORTS_HIDDEN)
ENDMACRO(CHECK_COMPILER_SUPPORTS_HIDDEN_D)

MACRO(CHECK_GETADDERINFO_WORKING_D)
	SET(GETADDERINFO_SRC "
	#include <netdb.h>
	int main () {
	const char *name, *port;
	struct addrinfo *aiPtr, hints;
	(void)getaddrinfo(name,port, &hints, &aiPtr);
	(void)freeaddrinfo(aiPtr);
	return 0;
	}")
	CHECK_C_SOURCE_COMPILES("${GETADDERINFO_SRC}" WORKING_GETADDERINFO)
	IF(WORKING_GETADDERINFO)
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DHAVE_GETADDERINFO=1" CACHE STRING "TCL CFLAGS" FORCE)
	ENDIF(WORKING_GETADDERINFO)
ENDMACRO(CHECK_GETADDERINFO_WORKING_D)


MACRO(TERMIOS_TERMIO_SGTTY)
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
		SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DUSE_TERMIOS=1" CACHE STRING "TCL CFLAGS" FORCE)
	ELSE(HAVE_TERMIOS)
		IF(HAVE_TERMIO)
			SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DUSE_TERMIO=1" CACHE STRING "TCL CFLAGS" FORCE)
		ELSE(HAVE_TERMIO)
			IF(HAVE_SGTTY)
				SET(${CFLAGS_NAME}_CFLAGS "${${CFLAGS_NAME}_CFLAGS} -DUSE_SGTTY=1" CACHE STRING "TCL CFLAGS" FORCE)
			ENDIF(HAVE_SGTTY)
		ENDIF(HAVE_TERMIO)
	ENDIF(HAVE_TERMIOS)


ENDMACRO(TERMIOS_TERMIO_SGTTY)

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

MACRO(CHECK_TIME_AND_SYS_TIME)
	SET(TEST_SRC "
	#include <sys/types.h>
	#include <sys/time.h>
	#include <time.h>
	int main()	{
	if ((struct tm *) 0)
		return 0;
	return 0;
	}
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" TIME_AND_SYS_TIME)
ENDMACRO(CHECK_TIME_AND_SYS_TIME)

