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

