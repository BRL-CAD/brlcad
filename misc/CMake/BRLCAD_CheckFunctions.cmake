#     B R L C A D _ C H E C K F U N C T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
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


###
# Check if a function exists (i.e., compiles to a valid symbol).  Adds
# HAVE_* define to config header.
###
MACRO(BRLCAD_FUNCTION_EXISTS function var)
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE AND ${var})
    FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_FUNCTION_EXISTS)


###
# Check if a header exists.  Headers requiring other headers being
# included first can be prepended in the filelist separated by
# semicolons.  Add HAVE_*_H define to config header.
###
MACRO(BRLCAD_INCLUDE_FILE filelist var)
  CHECK_INCLUDE_FILES("${filelist}" ${var})
  IF(CONFIG_H_FILE AND ${var})
    FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  ENDIF(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_INCLUDE_FILE)


###
# Check if a C++ header exists.  Adds HAVE_* define to config header.
###
MACRO(BRLCAD_INCLUDE_FILE_CXX filename var)
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  IF(CONFIG_H_FILE AND ${var})
    FILE(APPEND ${CONFIG_H_FILE} "#cmakedefine ${var} 1\n")
  ENDIF(CONFIG_H_FILE AND ${var})
ENDMACRO(BRLCAD_INCLUDE_FILE_CXX)


###
# Check the size of a given typename, setting the size in the provided
# variable.  If type has header prerequisites, a semicolon-separated
# header list may be specified.  Adds HAVE_ and SIZEOF_ defines to
# config header.
###
MACRO(BRLCAD_TYPE_SIZE typename var headers)
  SET(CMAKE_EXTRA_INCLUDE_FILES "${headers}")
  CHECK_TYPE_SIZE(${typename} HAVE_${var}_T)
  SET(CMAKE_EXTRA_INCLUDE_FILES)
  IF(CONFIG_H_FILE AND HAVE_${var}_T)
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var}_T 1\n")
    FILE(APPEND ${CONFIG_H_FILE} "#define SIZEOF_${var} ${HAVE_${var}_T}\n")
  ENDIF(CONFIG_H_FILE AND HAVE_${var}_T)
ENDMACRO(BRLCAD_TYPE_SIZE)


###
# Check if a given structure has a specific member element.  Structure
# should be defined within the semicolon-separated header file list.
# Adds HAVE_* to config header and sets VAR.
###
MACRO(BRLCAD_STRUCT_MEMBER structname member headers var)
  CHECK_STRUCT_HAS_MEMBER(${structname} ${member} "${headers}" HAVE_${var})
  IF(CONFIG_H_FILE AND HAVE_${var})
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_${var} 1\n")
  ENDIF(CONFIG_H_FILE AND HAVE_${var})
ENDMACRO(BRLCAD_STRUCT_MEMBER)


###
# Check if a given function exists in the specified library.  Sets
# targetname_LINKOPT variable as advanced if found.
###
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


###
# Undocumented.
###
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


###
# Undocumented.
###
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


###
# FIXME: Why are these here?
###
INCLUDE (CheckPrototypeExists)
INCLUDE (CheckCFileRuns)


###
# Undocumented.
# Based on AC_HEADER_SYS_WAIT
###
MACRO(BRLCAD_HEADER_SYS_WAIT)
  CHECK_C_FILE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/sys_wait_test.c WORKING_SYS_WAIT)
  IF(WORKING_SYS_WAIT)
    FILE(APPEND ${CONFIG_H_FILE} "#define HAVE_SYS_WAIT_H 1\n")
  ENDIF(WORKING_SYS_WAIT)
ENDMACRO(BRLCAD_HEADER_SYS_WAIT)

###
# Undocumented.
# Based on AC_FUNC_ALLOCA
###
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
# See if the compiler supports the C99 %z print specifier for size_t.
# Sets -DHAVE_STDINT_H=1 as global preprocessor flag if found.
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
  if (sprintf(buf, \"%zu\", (size_t)1234) != 4)
    return 1;
  else if (strcmp(buf, \"1234\"))
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
