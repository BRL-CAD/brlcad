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
	MESSAGE("- Found ${ARGV1} - setting to -${flag}")
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


# Automate putting variables from tests into CFLAGS,
# and otherwise wrap check macros in extra logic as needed
# These functions will do either of two jobs - if a
# CONFIG_H_FILE is defined, they will append cmakedefine
# (or occasionally straight define) statements to that file.
# If a CONFIG_CFLAGS variable is defined, they will append
# an appropriate definition to that variable.

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCSourceCompiles)

MACRO(CONFIG_CHECK_FUNCTION_EXISTS function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  IF(${var})
    add_definitions(-D${var}=1)
  ENDIF(${var})
ENDMACRO(CONFIG_CHECK_FUNCTION_EXISTS)

MACRO(CONFIG_CHECK_INCLUDE_FILE filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  IF(${var})
    add_definitions(-D${var}=1)
  ENDIF(${var})
ENDMACRO(CONFIG_CHECK_INCLUDE_FILE)

MACRO(CONFIG_CHECK_INCLUDE_FILE_USABILITY filename var)
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
  ENDIF(NOT HAVE_${var} OR NOT ${var}_USABLE)
ENDMACRO(CONFIG_CHECK_INCLUDE_FILE_USABILITY filename var)

MACRO(CONFIG_CHECK_INCLUDE_FILE_CXX filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  IF(${var})
    add_definitions(-D${var}=1)
  ENDIF(${var})
ENDMACRO(CONFIG_CHECK_INCLUDE_FILE_CXX)

MACRO(CONFIG_CHECK_TYPE_SIZE typename var)
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
ENDMACRO(CONFIG_CHECK_TYPE_SIZE)

MACRO(CONFIG_CHECK_STRUCT_HAS_MEMBER structname member header var)
  CHECK_STRUCT_HAS_MEMBER(${structname} ${member} ${header} HAVE_${var})
  IF(HAVE_${var})
    add_definitions(-DHAVE_${var}=1)
  ENDIF(HAVE_${var})
ENDMACRO(CONFIG_CHECK_STRUCT_HAS_MEMBER)

MACRO(CONFIG_CHECK_LIBRARY targetname lname func)
  IF(NOT ${targetname}_LIBRARY)
    CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
    IF(HAVE_${targetname}_${lname})
      set(${targetname}_LIBRARY "${lname}")
    ENDIF(HAVE_${targetname}_${lname})
  ENDIF(NOT ${targetname}_LIBRARY)
ENDMACRO(CONFIG_CHECK_LIBRARY lname func)

