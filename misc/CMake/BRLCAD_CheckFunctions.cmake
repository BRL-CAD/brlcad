#     B R L C A D _ C H E C K F U N C T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2022 United States Government as represented by
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

include(CMakeParseArguments)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckCInline)
include(CheckCSourceRuns)
include(CheckCXXSourceRuns)

set(standard_header_template
"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
typedef unsigned char u_char;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
# include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
"
)


# Use this function to construct compile defines that uses the CMake
# header tests results to properly support the above header includes
function(standard_header_cppflags header_cppflags)
  set(have_cppflags)
  string(REPLACE "\n" ";" template_line "${standard_header_template}")
  foreach(line ${template_line})
    if("${line}" MATCHES ".* HAVE_.*_H" )
      # extract the HAVE_* symbol from the #ifdef lines
      string(REGEX REPLACE "^[ ]*#[ ]*if.*[ ]+" "" have_symbol "${line}")
      if(NOT "${${have_symbol}}" MATCHES "^[\"]*$") # matches empty or ""
	set(have_cppflags "${have_cppflags} -D${have_symbol}=${${have_symbol}}")
      endif(NOT "${${have_symbol}}" MATCHES "^[\"]*$")
    endif("${line}" MATCHES ".* HAVE_.*_H" )
  endforeach(line ${template_line})
  set(${header_cppflags} "${have_cppflags}" PARENT_SCOPE)
endfunction(standard_header_cppflags)

# The macros and functions below need C_STANDARD_FLAGS to be set for most
# compilers - be sure it is in those cases
if("${C_STANDARD_FLAGS}" STREQUAL "")
  message(FATAL_ERROR "C_STANDARD_FLAGS is not set - should at least be defining the C standard")
endif("${C_STANDARD_FLAGS}" STREQUAL "")

###
# Check if a function exists (i.e., compiles to a valid symbol).  Adds
# HAVE_* define to config header, and HAVE_DECL_* and HAVE_WORKING_* if
# those tests are performed and successful.
###
macro(BRLCAD_FUNCTION_EXISTS function)

  # Use the upper case form of the function for variable names
  string(TOUPPER "${function}" var)

  # Only do the testing once per configure run
  if(NOT DEFINED HAVE_${var})

    # For this first test, be permissive.  For a number of cases, if
    # the function exists AT ALL we want to know it, so don't let the
    # flags get in the way
    set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "")

    # Clear our testing variables, unless something specifically requested
    # is supplied as a command line argument
    cmake_push_check_state(RESET)
    if(${ARGC} GREATER 2)
      # Parse extra arguments
      CMAKE_PARSE_ARGUMENTS(${var} "" "DECL_TEST_SRCS" "WORKING_TEST_SRCS;REQUIRED_LIBS;REQUIRED_DEFS;REQUIRED_FLAGS;REQUIRED_DIRS" ${ARGN})
      set(CMAKE_REQUIRED_LIBRARIES ${${var}_REQUIRED_LIBS})
      set(CMAKE_REQUIRED_FLAGS ${${var}_REQUIRED_FLAGS})
      set(CMAKE_REQUIRED_INCLUDES ${${var}_REQUIRED_DIRS})
      set(CMAKE_REQUIRED_DEFINITIONS ${${var}_REQUIRED_DEFS})
    endif(${ARGC} GREATER 2)

    # Set the compiler definitions for the standard headers
    if("${have_header_cppflags}" STREQUAL "")
      # get the cppflags once, the first time we test
      standard_header_cppflags(have_header_cppflags)
    endif("${have_header_cppflags}" STREQUAL "")
    set(CMAKE_REQUIRED_DEFINITIONS "${have_header_cppflags} ${CMAKE_REQUIRED_DEFINITIONS}")

    # First (permissive) test
    CHECK_FUNCTION_EXISTS(${function} HAVE_${var})

    # Now, restore the C flags - any subsequent tests will be done using the
    # parent C_FLAGS environment.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")

    if(HAVE_${var})

      # Test if the function is declared.This will set the HAVE_DECL_${var}
      # flag.  Unlike the HAVE_${var} results, this will not be automatically
      # written to the CONFIG_H_FILE - the caller must do so if they want to
      # capture the results.
      if(NOT DEFINED HAVE_DECL_${var})
	if(NOT MSVC)
	  if(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
	    if(ENABLE_ALL_CXX_COMPILE)
	      check_cxx_source_compiles("${${${var}_DECL_TEST_SRCS}}" HAVE_DECL_${var})
	    else(ENABLE_ALL_CXX_COMPILE)
	      check_c_source_compiles("${${${var}_DECL_TEST_SRCS}}" HAVE_DECL_${var})
	    endif(ENABLE_ALL_CXX_COMPILE)
	  else(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
	    if(ENABLE_ALL_CXX_COMPILE)
	      check_cxx_source_compiles("${standard_header_template}\nint main() {(void)${function}; return 0;}" HAVE_DECL_${var})
	    else(ENABLE_ALL_CXX_COMPILE)
	      check_c_source_compiles("${standard_header_template}\nint main() {(void)${function}; return 0;}" HAVE_DECL_${var})
	    endif(ENABLE_ALL_CXX_COMPILE)
	  endif(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
	else()
	  # Note - config_win.h (and probably other issues) make the decl tests
	  # a no-go with Visual Studio - punt until the larger issues are
	  # sorted out.
	  set(HAVE_DECL_${var} 1)
	endif(NOT MSVC)
	set(HAVE_DECL_${var} ${HAVE_DECL_${var}} CACHE BOOL "Cache decl test result")
	mark_as_advanced(HAVE_DECL_${var})
      endif(NOT DEFINED HAVE_DECL_${var})

      # If we have sources supplied for the purpose, test if the function is working.
      if(NOT "${${var}_COMPILE_TEST_SRCS}" STREQUAL "")
	if(NOT DEFINED HAVE_WORKING_${var})
	  set(HAVE_WORKING_${var} 1)
	  foreach(test_src ${${var}_COMPILE_TEST_SRCS})
	    if(ENABLE_ALL_CXX_COMPILE)
	      check_cxx_source_compiles("${${test_src}}" ${var}_${test_src}_COMPILE)
	    else(ENABLE_ALL_CXX_COMPILE)
	      check_c_source_compiles("${${test_src}}" ${var}_${test_src}_COMPILE)
	    endif(ENABLE_ALL_CXX_COMPILE)
	    if(NOT ${var}_${test_src}_COMPILE)
	      set(HAVE_WORKING_${var} 0)
	    endif(NOT ${var}_${test_src}_COMPILE)
	  endforeach(test_src ${${var}_COMPILE_TEST_SRCS})
	  set(HAVE_WORKING_${var} ${HAVE_DECL_${var}} CACHE BOOL "Cache working test result")
	  mark_as_advanced(HAVE_WORKING_${var})
	endif(NOT DEFINED HAVE_WORKING_${var})
      endif(NOT "${${var}_COMPILE_TEST_SRCS}" STREQUAL "")

    endif(HAVE_${var})

    cmake_pop_check_state()

  endif(NOT DEFINED HAVE_${var})

  # The config file is regenerated every time CMake is run, so we
  # always need this bit even if the testing is already complete.
  if(CONFIG_H_FILE AND HAVE_${var})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_${var} 1\n")
  endif(CONFIG_H_FILE AND HAVE_${var})
  if(CONFIG_H_FILE AND HAVE_DECL_${var})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_DECL_${var} 1\n")
  endif(CONFIG_H_FILE AND HAVE_DECL_${var})
  if(CONFIG_H_FILE AND HAVE_WORKING_${var})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_WORKING_${var} 1\n")
  endif(CONFIG_H_FILE AND HAVE_WORKING_${var})

endmacro(BRLCAD_FUNCTION_EXISTS)


###
# Check if a header exists.  Headers requiring other headers being
# included first can be prepended in the filelist separated by
# semicolons.  Add HAVE_*_H define to config header.
###
macro(BRLCAD_INCLUDE_FILE filelist var)

  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
  if(NOT "${ARGV2}" STREQUAL "")
    set(CMAKE_REQUIRED_INCLUDES ${ARGV2} ${CMAKE_REQUIRED_INCLUDES})
  endif(NOT "${ARGV2}" STREQUAL "")
  check_include_files("${filelist}" "${var}")
  cmake_pop_check_state()

  if(CONFIG_H_FILE AND ${var})
    CONFIG_H_APPEND(BRLCAD "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})

endmacro(BRLCAD_INCLUDE_FILE)


###
# Check if a C++ header exists.  Adds HAVE_* define to config header.
###
macro(BRLCAD_INCLUDE_FILE_CXX filename var)

  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_CXX_STD_FLAG}")
  check_include_file_cxx(${filename} ${var})
  cmake_pop_check_state()

  if(CONFIG_H_FILE AND ${var})
    CONFIG_H_APPEND(BRLCAD "#cmakedefine ${var} 1\n")
  endif(CONFIG_H_FILE AND ${var})

endmacro(BRLCAD_INCLUDE_FILE_CXX)


###
# Check the size of a given typename, setting the size in the provided
# variable.  If type has header prerequisites, a semicolon-separated header
# list may be specified.  Adds HAVE_ and SIZEOF_ defines to config header.
###
macro(BRLCAD_TYPE_SIZE typename headers)
  # Generate a variable name from the type - need to make sure we end up with a
  # valid variable string.
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var ${typename})
  string(TOUPPER "${var}" var)

  # To make sure checks are re-run when re-testing the same type with different
  # headers, create a test variable incorporating both the typename and the
  # headers string
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" testvar "HAVE_${typename}${headers}")
  string(TOUPPER "${testvar}" testvar)

  # Proceed with type check.
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
  set(CMAKE_EXTRA_INCLUDE_FILES "${headers}")
  check_type_size(${typename} ${testvar})
  cmake_pop_check_state()

  # Produce config.h lines as appropriate
  if(CONFIG_H_FILE AND ${testvar})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_${var} 1\n")
    CONFIG_H_APPEND(BRLCAD "#define SIZEOF_${var} ${${testvar}}\n")
  endif(CONFIG_H_FILE AND ${testvar})

endmacro(BRLCAD_TYPE_SIZE)


###
# Check if a given structure has a specific member element.  Structure
# should be defined within the semicolon-separated header file list.
# Adds HAVE_* to config header and sets VAR.
###
macro(BRLCAD_STRUCT_MEMBER structname member headers var)

  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")

  check_struct_has_member(${structname} ${member} "${headers}" HAVE_${var})

  cmake_pop_check_state()

  if(CONFIG_H_FILE AND HAVE_${var})
    CONFIG_H_APPEND(BRLCAD "#define HAVE_${var} 1\n")
  endif(CONFIG_H_FILE AND HAVE_${var})

endmacro(BRLCAD_STRUCT_MEMBER)


###
# Check if a given function exists in the specified library.
###
macro(BRLCAD_CHECK_LIBRARY targetname lname func)

  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")

  # Don't error out on the builtin conflict case
  check_c_compiler_flag(-fno-builtin HAVE_NO_BUILTIN)
  if(HAVE_NO_BUILTIN)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fno-builtin")
  endif(HAVE_NO_BUILTIN)

  if(NOT ${targetname}_LIBRARY)
    check_library_exists(${lname} ${func} "" HAVE_${targetname}_${lname})
  else(NOT ${targetname}_LIBRARY)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${${targetname}_LIBRARY})
    check_function_exists(${func} HAVE_${targetname}_${lname})
  endif(NOT ${targetname}_LIBRARY)
  cmake_pop_check_state()

  if(HAVE_${targetname}_${lname})
    set(${targetname}_LIBRARY "${lname}")
  endif(HAVE_${targetname}_${lname})

endmacro(BRLCAD_CHECK_LIBRARY lname func)


# Special purpose functions

###
# Undocumented.
###
function(BRLCAD_CHECK_BASENAME)

  set(BASENAME_SRC "
#include <libgen.h>
int main(int argc, char *argv[]) {
if(!argc) return 1;
(void)basename(argv[0]);
return 0;
}")
  if(NOT DEFINED HAVE_BASENAME)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${BASENAME_SRC}" HAVE_BASENAME)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${BASENAME_SRC}" HAVE_BASENAME)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_BASENAME)
  if(HAVE_BASENAME)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_BASENAME 1\n")
  endif(HAVE_BASENAME)

endfunction(BRLCAD_CHECK_BASENAME var)

###
# Undocumented.
###
function(BRLCAD_CHECK_DIRNAME)
  set(DIRNAME_SRC "
#include <libgen.h>
int main(int argc, char *argv[]) {
if(!argc) return 1;
(void)dirname(argv[0]);
return 0;
}")
  if(NOT DEFINED HAVE_DIRNAME)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${DIRNAME_SRC}" HAVE_DIRNAME)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${DIRNAME_SRC}" HAVE_DIRNAME)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_DIRNAME)
  if(HAVE_DIRNAME)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_DIRNAME 1\n")
  endif(HAVE_DIRNAME)
endfunction(BRLCAD_CHECK_DIRNAME var)

###
# Undocumented.
# Based on AC_HEADER_SYS_WAIT
###
function(BRLCAD_HEADER_SYS_WAIT)
  set(SYS_WAIT_SRC "
#include <sys/types.h>
#include <sys/wait.h>
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned int) (stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
int main() {
  int s;
  wait(&s);
  s = WIFEXITED(s) ? WEXITSTATUS(s) : 1;
  return 0;
}")
  if(NOT DEFINED WORKING_SYS_WAIT)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${SYS_WAIT_SRC}" WORKING_SYS_WAIT)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${SYS_WAIT_SRC}" WORKING_SYS_WAIT)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED WORKING_SYS_WAIT)
  if(WORKING_SYS_WAIT)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_SYS_WAIT_H 1\n")
  endif(WORKING_SYS_WAIT)
endfunction(BRLCAD_HEADER_SYS_WAIT)

###
# Undocumented.
# Based on AC_FUNC_ALLOCA
###
function(BRLCAD_ALLOCA)
  set(ALLOCA_HEADER_SRC "
#include <alloca.h>
int main() {
   char *p = (char *) alloca (2 * sizeof (int));
   if (p) return 0;
   ;
   return 0;
}")
  set(ALLOCA_TEST_SRC "
#ifdef __GNUC__
# define alloca __builtin_alloca
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#  else
#   ifdef _AIX
 #pragma alloca
#   else
#    ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#    endif
#   endif
#  endif
# endif
#endif

int main() {
   char *p = (char *) alloca (1);
   if (p) return 0;
   ;
   return 0;
}")
  if(WORKING_ALLOC_H STREQUAL "")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${ALLOCA_HEADER_SRC}" WORKING_ALLOCA_H)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${ALLOCA_HEADER_SRC}" WORKING_ALLOCA_H)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
    set(WORKING_ALLOCA_H ${WORKING_ALLOCA_H} CACHE INTERNAL "alloca_h test")
  endif(WORKING_ALLOC_H STREQUAL "")
  if(WORKING_ALLOCA_H)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ALLOCA_H 1\n")
    set(FILE_RUN_DEFINITIONS "-DHAVE_ALLOCA_H")
  endif(WORKING_ALLOCA_H)
  if(NOT DEFINED WORKING_ALLOCA)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${ALLOCA_TEST_SRC}" WORKING_ALLOCA)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${ALLOCA_TEST_SRC}" WORKING_ALLOCA)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED WORKING_ALLOCA)
  if(WORKING_ALLOCA)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ALLOCA 1\n")
  endif(WORKING_ALLOCA)
endfunction(BRLCAD_ALLOCA)

###
# See if the compiler supports the C99 %z print specifier for size_t.
# Sets -DHAVE_STDINT_H=1 as global preprocessor flag if found.
###
function(BRLCAD_CHECK_PERCENT_Z)

  check_include_file(stdint.h HAVE_STDINT_H)

  set(CHECK_PERCENT_Z_SRC "
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif
#include <stdio.h>
int main(int ac, char *av[])
{
  char buf[64] = {0};
  if (sprintf(buf, \"%zu\", (size_t)1) != 1 || buf[0] != '1' || buf[1] != 0)
    return 1;
  return (ac < 0) ? (int)av[0][0] : 0;
}
")

  if(NOT DEFINED HAVE_PERCENT_Z)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(HAVE_STDINT_H)
      set(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_STDINT_H=1")
    endif(HAVE_STDINT_H)
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${CHECK_PERCENT_Z_SRC}" HAVE_PERCENT_Z)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${CHECK_PERCENT_Z_SRC}" HAVE_PERCENT_Z)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_PERCENT_Z)

  if(HAVE_PERCENT_Z)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_PERCENT_Z 1\n")
  endif(HAVE_PERCENT_Z)
endfunction(BRLCAD_CHECK_PERCENT_Z)


function(BRLCAD_CHECK_STATIC_ARRAYS)
  set(CHECK_STATIC_ARRAYS_SRC "
#include <stdio.h>
#include <string.h>

int foobar(char arg[static 100])
{
  return (int)arg[0];
}

int main(int ac, char *av[])
{
  char hello[100];

  if (ac > 0 && av)
    foobar(hello);
  return 0;
}
")
  if(NOT DEFINED HAVE_STATIC_ARRAYS)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${CHECK_STATIC_ARRAYS_SRC}" HAVE_STATIC_ARRAYS)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${CHECK_STATIC_ARRAYS_SRC}" HAVE_STATIC_ARRAYS)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_STATIC_ARRAYS)
  if(HAVE_STATIC_ARRAYS)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_STATIC_ARRAYS 1\n")
  endif(HAVE_STATIC_ARRAYS)
endfunction(BRLCAD_CHECK_STATIC_ARRAYS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
