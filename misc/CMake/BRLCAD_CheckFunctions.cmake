# Automate putting variables from tests into a config.h.in file,
# and otherwise wrap check macros in extra logic as needed

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructHasMember)
INCLUDE(ResolveCompilerPaths)

MACRO(BRLCAD_FUNCTION_EXISTS function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_FUNCTION_EXISTS)

MACRO(BRLCAD_INCLUDE_FILE filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_INCLUDE_FILE)

MACRO(BRLCAD_INCLUDE_FILE_CXX filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  if(CONFIG_H_FILE AND ${var})
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_INCLUDE_FILE_CXX)

MACRO(BRLCAD_TYPE_SIZE typename var header)
	SET(CMAKE_EXTRA_INCLUDE_FILES ${header})
	CHECK_TYPE_SIZE(${typename} HAVE_${var}_T)
	SET(CMAKE_EXTRA_INCLUDE_FILES)
	if(CONFIG_H_FILE AND HAVE_${var}_T)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var}_T 1\n")
		FILE(APPEND ${CONFIG_H_FILE} "#define SIZEOF_${var} ${HAVE_${var}_T}\n")
	endif(CONFIG_H_FILE AND HAVE_${var}_T)
ENDMACRO(BRLCAD_TYPE_SIZE)

MACRO(BRLCAD_STRUCT_MEMBER structname member header var)
	CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} HAVE_${var})
	if(CONFIG_H_FILE AND HAVE_${var})
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var} 1\n")
	endif(CONFIG_H_FILE AND HAVE_${var})
ENDMACRO(BRLCAD_STRUCT_MEMBER)

MACRO(BRLCAD_CHECK_LIBRARY targetname lname func)
	IF(NOT ${targetname}_LIBRARY)
		CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
		IF(HAVE_${targetname}_${lname})
			RESOLVE_LIBRARIES (${targetname}_LIBRARY "-l${lname}")
			SET(${targetname}_LINKOPT "-l${lname}" CACHE STRING "${targetname} link option")
			MARK_AS_ADVANCED(${targetname}_LINKOPT)
		ENDIF(HAVE_${targetname}_${lname})
	ENDIF(NOT ${targetname}_LIBRARY)
ENDMACRO(BRLCAD_CHECK_LIBRARY lname func)

# Special purpose macros

INCLUDE(CheckCSourceRuns)

MACRO(BRLCAD_CHECK_BASENAME)
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
ENDMACRO(BRLCAD_CHECK_BASENAME var)

MACRO(BRLCAD_CHECK_DIRNAME)
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
ENDMACRO(BRLCAD_CHECK_DIRNAME var)

INCLUDE (CheckPrototypeExists)
INCLUDE (CheckCFileRuns)
# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file.  Named using
# BRL-CAD prefix to avoid any confusion with similar tests in
# other directories, which may get loaded first.
MACRO(BRLCAD_HEADER_STDC)
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
  CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
    FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(BRLCAD_HEADER_STDC)

# Based on AC_HEADER_SYS_WAIT
MACRO(BRLCAD_HEADER_SYS_WAIT)
  CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/sys_wait_test.c WORKING_SYS_WAIT)
  IF(WORKING_SYS_WAIT)
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_SYS_WAIT_H 1\n")
  ENDIF(WORKING_SYS_WAIT)
ENDMACRO(BRLCAD_HEADER_SYS_WAIT)

# Based on AC_FUNC_ALLOCA
MACRO(BRLCAD_ALLOCA)
	CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/alloca_header_test.c WORKING_ALLOCA_H)
	IF(WORKING_ALLOCA_H)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_ALLOCA_H 1\n")
		SET(FILE_RUN_DEFINITIONS "-DHAVE_ALLOCA_H")
	ENDIF(WORKING_ALLOCA_H)
	CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/alloca_test.c WORKING_ALLOCA)
	IF(WORKING_ALLOCA)
		FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_ALLOCA 1\n")
	ENDIF(WORKING_ALLOCA)
ENDMACRO(BRLCAD_ALLOCA)

###
# See if the compiler supports the C99 %z print specifier for size_t
###
MACRO(BRLCAD_CHECK_C99_FORMAT_SPECIFIERS)
	SET(CMAKE_REQUIRED_DEFINITIONS_BAK ${CMAKE_REQUIRED_DEFINITIONS})
	CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
	IF(HAVE_STDINT_H)
		SET(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_STDINT_H=1")
	ENDIF(HAVE_STDINT_H)
  SET(CHECK_C99_FORMAT_SPECIFIERS_SRC "
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif
#include <stdio.h>
#include <string.h>
int main(int ac, char *av[])
{
  char buf[64] = {0};
  if (sprintf(buf, \"%zu\", (size_t)123) != 3)
    return 1;
  else if (strcmp(buf, \"123\"))
    return 2;
  return 0;
}
")
  CHECK_C_SOURCE_RUNS("${CHECK_C99_FORMAT_SPECIFIERS_SRC}" HAVE_C99_FORMAT_SPECIFIERS)
  IF(HAVE_C99_FORMAT_SPECIFIERS)
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_C99_FORMAT_SPECIFIERS 1\n")
  ENDIF(HAVE_C99_FORMAT_SPECIFIERS)
  SET(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS_BAK})
ENDMACRO(BRLCAD_CHECK_C99_FORMAT_SPECIFIERS)
