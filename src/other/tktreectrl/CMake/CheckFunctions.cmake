# Automate putting variables from tests into a config.h.in file,
# and otherwise wrap check macros in extra logic as needed

INCLUDE(CheckIncludeFile)
INCLUDE(CheckPrototypeExists)
INCLUDE(CheckCFileRuns)
# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file
MACRO(CMAKE_HEADER_STDC)
  CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
  CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
  CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
  CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
  CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
  CHECK_PROTOTYPE_EXISTS(memchr string.h HAVE_STRING_H_MEMCHR)
  CHECK_PROTOTYPE_EXISTS(free stdlib.h HAVE_STDLIB_H_FREE)
  CHECK_C_FILE_RUNS(${CMAKE_CURRENT_SOURCE_DIR}/CMake/test_srcs/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
	  SET(STDC_HEADERS 1)
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(CMAKE_HEADER_STDC)

