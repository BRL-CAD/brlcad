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

MACRO(CHECK_FUNCTION_EXISTS_H function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(CHECK_FUNCTION_EXISTS_H)

MACRO(CHECK_INCLUDE_FILE_H filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(CHECK_INCLUDE_FILE_H)

MACRO(CHECK_INCLUDE_FILE_USABILITY_H filename var)
	CHECK_INCLUDE_FILE_H(${filename} HAVE_${var})
	IF(HAVE_${var})
		SET(HEADER_SRC "
		#include <${filename}>
		main(){};
		")
		CHECK_C_SOURCE_COMPILES("${HEADER_SRC}" ${var}_USABLE)
	ENDIF(HAVE_${var})
	IF(NOT HAVE_${var} OR NOT ${var}_USABLE)
		IF(CONFIG_H_FILE)
			FILE(APPEND ${CONFIG_H_FILE} "#define NO_${var} 1\n")
		ENDIF(CONFIG_H_FILE)
	ENDIF(NOT HAVE_${var} OR NOT ${var}_USABLE)
ENDMACRO(CHECK_INCLUDE_FILE_USABILITY_H filename var)

MACRO(CHECK_INCLUDE_FILE_CXX_H filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(CHECK_INCLUDE_FILE_CXX_H)

MACRO(CHECK_TYPE_SIZE_H typename var)
	FOREACH(arg ${ARGN})
		SET(headers ${headers} ${arg})
	ENDFOREACH(arg ${ARGN})
	SET(CHECK_EXTRA_INCLUDE_FILES ${headers})
	CHECK_TYPE_SIZE(${typename} HAVE_${var}_T)
	SET(CHECK_EXTRA_INCLUDE_FILES)
	if(CONFIG_H_FILE AND HAVE_${var}_T)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var}_T 1\n")
		FILE(APPEND ${CONFIG_H_FILE} "#define SIZEOF_${var} ${HAVE_${var}_T}\n")
	endif(CONFIG_H_FILE AND HAVE_${var}_T)
ENDMACRO(CHECK_TYPE_SIZE_H)

MACRO(CHECK_STRUCT_HAS_MEMBER_H structname member header var)
	CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} HAVE_${var})
	if(CONFIG_H_FILE AND HAVE_${var})
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var} 1\n")
	endif(CONFIG_H_FILE AND HAVE_${var})
ENDMACRO(CHECK_STRUCT_HAS_MEMBER_H)

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

MACRO(CHECK_BASENAME)
	SET(BASENAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)basename(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${BASENAME_SRC}" HAVE_BASENAME)
	IF(HAVE_BASENAME)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_BASENAME 1\n")
	ENDIF(HAVE_BASENAME)
ENDMACRO(CHECK_BASENAME var)

MACRO(CHECK_DIRNAME)
	SET(DIRNAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)dirname(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${DIRNAME_SRC}" HAVE_DIRNAME)
	IF(HAVE_DIRNAME)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_DIRNAME 1\n")
	ENDIF(HAVE_DIRNAME)
ENDMACRO(CHECK_DIRNAME var)

# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file
MACRO(CMAKE_HEADER_STDC)
  CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
  FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_STDLIB_H 1\n")
  CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
  CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
  FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_STRING_H 1\n")
  CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
  FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_STRINGS_H 1\n")
  CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
  CHECK_PROTOTYPE_EXISTS(memchr string.h HAVE_STRING_H_MEMCHR)
  CHECK_PROTOTYPE_EXISTS(free stdlib.h HAVE_STDLIB_H_FREE)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
    FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(CMAKE_HEADER_STDC)

# Based on AC_HEADER_SYS_WAIT
MACRO(CMAKE_HEADER_SYS_WAIT)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/sys_wait_test.c WORKING_SYS_WAIT)
  IF(WORKING_SYS_WAIT)
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_SYS_WAIT_H 1\n")
  ENDIF(WORKING_SYS_WAIT)
ENDMACRO(CMAKE_HEADER_SYS_WAIT)

# Based on AC_FUNC_ALLOCA
MACRO(CMAKE_ALLOCA)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_header_test.c WORKING_ALLOCA_H)
	IF(WORKING_ALLOCA_H)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_ALLOCA_H 1\n")
		SET(FILE_RUN_DEFINITIONS "-DHAVE_ALLOCA_H")
	ENDIF(WORKING_ALLOCA_H)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_test.c WORKING_ALLOCA)
	IF(WORKING_ALLOCA)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_ALLOCA 1\n")
	ENDIF(WORKING_ALLOCA)
ENDMACRO(CMAKE_ALLOCA)

MACRO(CHECK_COMPILER_SUPPORTS_HIDDEN)
	SET(TEST_SRC"
	#define MODULE_SCOPE extern __attribute__((__visibility__("hidden")))
	main(){};
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" COMPILER_SUPPORTS_HIDDEN)
ENDMACRO(CHECK_COMPILER_SUPPORTS_HIDDEN)

MACRO(CHECK_GETADDERINFO_WORKING)
	SET(GETADDERINFO_SRC "
	#include <netdb.h>
	int main () {
	const char *name, *port;
	struct addrinfo *aiPtr, hints;
	(void)getaddrinfo(name,port, &hints, &aiPtr);
	(void)freeaddrinfo(aiPtr);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${GETADDERINFO_SRC}" WORKING_GETADDERINFO)
	IF(WORKING_GETADDERINFO)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_GETADDERINFO 1\n")
	ENDIF(WORKING_GETADDERINFO)
ENDMACRO(CHECK_GETADDERINFO_WORKING)


MACRO(TERMIOS_TERMIO_SGTTY)
	SET(TERMIOS_SRC "
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
	SET(TERMIO_SRC "
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
	SET(SGTTY_SRC "
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
	CHECK_C_SOURCE_RUNS("${TERMIOS_SRC}" HAVE_TERMIOS)
	IF(NOT HAVE_TERMIOS)
		CHECK_C_SOURCE_RUNS("${TERMIO_SRC}" HAVE_TERMIO)
	ENDIF(NOT HAVE_TERMIOS)
	IF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
		CHECK_C_SOURCE_RUNS("${SGTTY_SRC}" HAVE_SGTTY)
	ENDIF(NOT HAVE_TERMIO AND NOT HAVE_TERMIOS)
ENDMACRO(TERMIOS_TERMIO_SGTTY)

MACRO(CHECK_FD_SET_IN_TYPES_H)
	SET(TEST_SRC "
	#include <sys/types.h>
	int main ()
	{
	fd_set readMask, writeMask;
	return 0;
	}
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" FD_SET_IN_TYPES_H)
ENDMACRO(CHECK_FD_SET_IN_TYPES_H)

MACRO(CHECK_TIME_AND_SYS_TIME)
	SET(TEST_SRC "
	#include <sys/types.h>
	#include <sys/time.h>
	#include <time.h>

	int
	main ()
	{
	if ((struct tm *) 0)
		return 0;
	return 0;
	}
	")
	CHECK_C_SOURCE_COMPILES("${TEST_SRC}" TIME_AND_SYS_TIME)
ENDMACRO(CHECK_TIME_AND_SYS_TIME)

