#     B R L C A D _ C H E C K F U N C T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckCInline)


###
# Check if a function exists (i.e., compiles to a valid symbol).  Adds
# HAVE_* define to config header.
###
macro(BRLCAD_FUNCTION_EXISTS function var)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  CHECK_FUNCTION_EXISTS(${function} ${var})
  if(CONFIG_H_FILE AND ${var})
    CONFIG_H_APPEND(BRLCAD "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_FUNCTION_EXISTS)


###
# Check if a header exists.  Headers requiring other headers being
# included first can be prepended in the filelist separated by
# semicolons.  Add HAVE_*_H define to config header.
###
macro(BRLCAD_INCLUDE_FILE filelist var)
  # check with no flags set
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")

  # !!! why doesn't this work?
  # if("${var}" STREQUAL "HAVE_X11_XLIB_H")
  #  message("CMAKE_REQUIRED_INCLUDES for ${var} is ${CMAKE_REQUIRED_INCLUDES}")
  # endif("${var}" STREQUAL "HAVE_X11_XLIB_H")

  if(NOT "${ARGV2}" STREQUAL "")
    set(CMAKE_REQUIRED_INCLUDES_BKUP ${CMAKE_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_INCLUDES ${ARGV2} ${CMAKE_REQUIRED_INCLUDES})
  endif(NOT "${ARGV2}" STREQUAL "")

  # search for the header
  CHECK_INCLUDE_FILES("${filelist}" ${var})

  # !!! curiously strequal matches true above for all the NOT ${var} cases
  # if (NOT ${var})
  #   message("--- ${var}")
  # endif (NOT ${var})

  if(CONFIG_H_FILE AND ${var})
    CONFIG_H_APPEND(BRLCAD "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})

  if(NOT "${ARGV2}" STREQUAL "")
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES_BKUP})
  endif(NOT "${ARGV2}" STREQUAL "")

  # restore flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_INCLUDE_FILE)


###
# Check if a C++ header exists.  Adds HAVE_* define to config header.
###
macro(BRLCAD_INCLUDE_FILE_CXX filename var)
  set(CMAKE_CXX_FLAGS_TMP "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "")
  CHECK_INCLUDE_FILE_CXX(${filename} ${var})
  if(CONFIG_H_FILE AND ${var})
    CONFIG_H_APPEND(BRLCAD "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_TMP}")
endmacro(BRLCAD_INCLUDE_FILE_CXX)


###
# Check the size of a given typename, setting the size in the provided
# variable.  If type has header prerequisites, a semicolon-separated
# header list may be specified.  Adds HAVE_ and SIZEOF_ defines to
# config header.
###
macro(BRLCAD_TYPE_SIZE typename headers)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  set(CMAKE_EXTRA_INCLUDE_FILES_BAK "${CMAKE_EXTRA_INCLUDE_FILES}")
  set(CMAKE_EXTRA_INCLUDE_FILES "${headers}")
  # Generate a variable name from the type - need to make sure
  # we end up with a valid variable string.
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var ${typename})
  string(TOUPPER "${var}" var)
  # Proceed with type check.  To make sure checks are re-run when
  # re-testing the same type with different headers, create a test
  # variable incorporating both the typename and the headers string
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" testvar "HAVE_${typename}${headers}")
  string(TOUPPER "${testvar}" testvar)
  CHECK_TYPE_SIZE(${typename} ${testvar})
  set(CMAKE_EXTRA_INCLUDE_FILES "${CMAKE_EXTRA_INCLUDE_FILES_BAK}")
  # Produce config.h lines as appropriate
  if(CONFIG_H_FILE AND ${testvar})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_${var} 1\n")
    CONFIG_H_APPEND(BRLCAD "#define SIZEOF_${var} ${${testvar}}\n")
  endif(CONFIG_H_FILE AND ${testvar})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_TYPE_SIZE)


###
# Check if a given structure has a specific member element.  Structure
# should be defined within the semicolon-separated header file list.
# Adds HAVE_* to config header and sets VAR.
###
macro(BRLCAD_STRUCT_MEMBER structname member headers var)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  CHECK_STRUCT_HAS_MEMBER(${structname} ${member} "${headers}" HAVE_${var})
  if(CONFIG_H_FILE AND HAVE_${var})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_${var} 1\n")
  endif(CONFIG_H_FILE AND HAVE_${var})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_STRUCT_MEMBER)


###
# Check if a given function exists in the specified library.  Sets
# targetname_LINKOPT variable as advanced if found.
###
macro(BRLCAD_CHECK_LIBRARY targetname lname func)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  if(NOT ${targetname}_LIBRARY)
    CHECK_LIBRARY_EXISTS(${lname} ${func} "" HAVE_${targetname}_${lname})
    if(HAVE_${targetname}_${lname})
      set(${targetname}_LIBRARY "${lname}")
    endif(HAVE_${targetname}_${lname})
  endif(NOT ${targetname}_LIBRARY)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_CHECK_LIBRARY lname func)


# Special purpose macros

# Load local variation on CHECK_C_SOURCE_RUNS that will accept a
# C file as well as the actual C code - some tests are easier to
# define in separate files.  This feature has been submitted back
# to the CMake project, but as of CMake 2.8.7 is not part of the
# default CHECK_C_SOURCE_RUNS functionality.
include(CheckCSourceRuns)

###
# Undocumented.
###
macro(BRLCAD_CHECK_BASENAME)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  set(BASENAME_SRC "
#include <libgen.h>
int main(int argc, char *argv[]) {
(void)basename(argv[0]);
return 0;
}")
  CHECK_C_SOURCE_RUNS("${BASENAME_SRC}" HAVE_BASENAME)
  if(HAVE_BASENAME)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_BASENAME 1\n")
  endif(HAVE_BASENAME)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_CHECK_BASENAME var)

###
# Undocumented.
###
macro(BRLCAD_CHECK_DIRNAME)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  set(DIRNAME_SRC "
#include <libgen.h>
int main(int argc, char *argv[]) {
(void)dirname(argv[0]);
return 0;
}")
  CHECK_C_SOURCE_RUNS("${DIRNAME_SRC}" HAVE_DIRNAME)
  if(HAVE_DIRNAME)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_DIRNAME 1\n")
  endif(HAVE_DIRNAME)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_CHECK_DIRNAME var)

###
# Undocumented.
# Based on AC_HEADER_SYS_WAIT
###
macro(BRLCAD_HEADER_SYS_WAIT)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  CHECK_C_SOURCE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/sys_wait_test.c WORKING_SYS_WAIT)
  if(WORKING_SYS_WAIT)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_SYS_WAIT_H 1\n")
  endif(WORKING_SYS_WAIT)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_HEADER_SYS_WAIT)

###
# Undocumented.
# Based on AC_FUNC_ALLOCA
###
macro(BRLCAD_ALLOCA)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  CHECK_C_SOURCE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/alloca_header_test.c WORKING_ALLOCA_H)
  if(WORKING_ALLOCA_H)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ALLOCA_H 1\n")
    set(FILE_RUN_DEFINITIONS "-DHAVE_ALLOCA_H")
  endif(WORKING_ALLOCA_H)
  CHECK_C_SOURCE_RUNS(${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/alloca_test.c WORKING_ALLOCA)
  if(WORKING_ALLOCA)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ALLOCA 1\n")
  endif(WORKING_ALLOCA)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_ALLOCA)


###
# See if the compiler supports the C99 %z print specifier for size_t.
# Sets -DHAVE_STDINT_H=1 as global preprocessor flag if found.
###
macro(BRLCAD_CHECK_C99_FORMAT_SPECIFIERS)
  set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "")
  set(CMAKE_REQUIRED_DEFINITIONS_BAK ${CMAKE_REQUIRED_DEFINITIONS})
  CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
  if(HAVE_STDINT_H)
    set(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_STDINT_H=1")
  endif(HAVE_STDINT_H)
  set(CHECK_C99_FORMAT_SPECIFIERS_SRC "
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
  if(HAVE_C99_FORMAT_SPECIFIERS)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_C99_FORMAT_SPECIFIERS 1\n")
  endif(HAVE_C99_FORMAT_SPECIFIERS)
  set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS_BAK})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")
endmacro(BRLCAD_CHECK_C99_FORMAT_SPECIFIERS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
