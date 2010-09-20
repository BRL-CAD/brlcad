# Automate putting variables from tests into a config.h.in file,
# and otherwise wrap check macros in extra logic as needed

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(ResolveCompilerPaths)

MACRO(BRLCAD_FUNCTION_EXISTS function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_FUNCTION_EXISTS)

MACRO(BRLCAD_INCLUDE_FILE filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_INCLUDE_FILE)

MACRO(BRLCAD_INCLUDE_FILE_CXX filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_INCLUDE_FILE_CXX)

MACRO(BRLCAD_TYPE_SIZE typename var)
  CHECK_TYPE_SIZE(${typename} ${var})
  if(CONFIG_H_FILE)
     FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine HAVE_${var} 1\n")
  endif(CONFIG_H_FILE)
ENDMACRO(BRLCAD_TYPE_SIZE)

MACRO(BRLCAD_CHECK_LIBRARY targetname lname func)
	IF(NOT ${targetname}_LIBRARY)
		CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
		IF(HAVE_${targetname}_${lname})
			RESOLVE_LIBRARIES (${targetname}_LIBRARY "-l${lname}")
			SET(${targetname}_LINKOPT "-l${lname}")
		ENDIF(HAVE_${targetname}_${lname})
	ENDIF(NOT ${targetname}_LIBRARY)
ENDMACRO(BRLCAD_CHECK_LIBRARY lname func)

# Special purpose macros

INCLUDE(CheckCSourceRuns)

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

INCLUDE (CheckPrototypeExists)
INCLUDE (CheckCFileRuns)
# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file
MACRO(CMAKE_HEADER_STDC)
  CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
  CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
  CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
  CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
  CHECK_PROTOTYPE_EXISTS(memchr string.h HAVE_STRING_H_MEMCHR)
  CHECK_PROTOTYPE_EXISTS(free stdlib.h HAVE_STDLIB_H_FREE)
  CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
    FILE(APPEND ${CONFIG_H_FILE} "#define STDC_HEADERS 1\n")
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(CMAKE_HEADER_STDC)
