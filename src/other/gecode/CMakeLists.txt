#
#  Main authors:
#     Victor Zverovich <victor.zverovich@gmail.com>
#
#  Copyright:
#     Victor Zverovich, 2013
#
#  Last modified:
#     $Date$ by $Author$
#     $Revision$
#
#  This file is part of Gecode, the generic constraint
#  development environment:
#     http://www.gecode.org
#
#  Permission is hereby granted, free of charge, to any person obtaining
#  a copy of this software and associated documentation files (the
#  "Software"), to deal in the Software without restriction, including
#  without limitation the rights to use, copy, modify, merge, publish,
#  distribute, sublicense, and/or sell copies of the Software, and to
#  permit persons to whom the Software is furnished to do so, subject to
#  the following conditions:
#
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
#  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
#  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
#  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

#
# CMake build script for Gecode.
#

cmake_minimum_required(VERSION 2.8.8)

project(GECODE)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include(CheckCXXCompilerFlag)
if (GECODE_DISABLE_WARNINGS)
  if (MSVC)
    add_definitions(/wd4244 /wd4267 /wd4345 /wd4355 /wd4800)
  else ()
    foreach (flag -Wextra -Wall -pedantic)
      string(REPLACE ${flag} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endforeach ()
    if (CMAKE_COMPILER_IS_GNUCXX)
      add_definitions(-Wno-overloaded-virtual)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      add_definitions(-Wno-constant-logical-operand -Wno-switch)
    endif ()
  endif ()
endif ()

# The following part of config.h is hard to derive from configure.ac.
file(READ gecode/support/config.hpp.in CONFIG)
string(REGEX REPLACE "^/\\*([^*]|\\*[^/])*\\*/" "" CONFIG ${CONFIG})
string(REGEX MATCHALL "[^\n]*\n" CONFIG
"${CONFIG}
/* Define to 1 if you have the `getpagesize' function. */
#undef HAVE_GETPAGESIZE

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have a working `mmap' system call. */
#undef HAVE_MMAP

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the <sys/param.h> header file. */
#undef HAVE_SYS_PARAM_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* The size of `int', as computed by sizeof. */
#undef SIZEOF_INT

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS
")

# Get version numbers and parts of config.h from configure.ac.
file(READ configure.ac LINES)
# Replace semicolons with "<semi>" to avoid CMake messing with them.
string(REPLACE ";" "<semi>" LINES "${LINES}")
# Split into lines keeping newlines to avoid foreach skipping empty ones.
string(REGEX MATCHALL "[^\n]*\n" LINES "${LINES}")
set(ah_command FALSE)
foreach (line "${EXTRA_CONFIG}" ${LINES})
  string(REPLACE ";" "" line "${line}")
  if (ah_command)
    # Do nothing.
  elseif (line MATCHES "AC_INIT\\(([^,]*), *([^,]*), *([^)]*)\\)")
    set(PACKAGE ${CMAKE_MATCH_1})
    set(VERSION ${CMAKE_MATCH_2})
    set(PACKAGE_BUGREPORT ${CMAKE_MATCH_3})
    message(STATUS "Got VERSION=${VERSION} from configure.ac")
  elseif (line MATCHES "ac_gecode_flatzincversion=(.*)\n")
    set(GECODE_FLATZINC_VERSION "${CMAKE_MATCH_1}")
  elseif (line MATCHES "AH_BOTTOM\\(\\[(.*)")
    set(ah_command bottom)
    set(line "${CMAKE_MATCH_1}")
  elseif (line MATCHES "AH_VERBATIM[^,]+,(.*)")
    set(ah_command verbatim)
    set(line "${CMAKE_MATCH_1}")
  endif ()
  if (ah_command)
    set(saved_ah_command ${ah_command})
    if (line MATCHES "^\\[(.*)")
      set(line "${CMAKE_MATCH_1}")
    endif ()
    if (line MATCHES "\\]\\)")
      set(ah_command FALSE)
      string(REPLACE "])" "" line "${line}")
    endif ()
    # For some reason CMake may bundle several lines together. Split them too.
    string(REGEX MATCHALL "[^\n]*\n" sublines "${line}")
    set(config_add "")
    foreach (subline ${sublines})
      set(config_add ${config_add} "${subline}")
    endforeach ()
    if (saved_ah_command STREQUAL "verbatim")
      set(CONFIG ${config_add} ${CONFIG})
    else ()
      set(CONFIG ${CONFIG} "\n" ${config_add})
    endif ()
  endif ()
endforeach ()
set(PACKAGE_NAME ${PACKAGE})
string(TOLOWER ${PACKAGE} PACKAGE_TARNAME)
set(PACKAGE_URL "")
set(PACKAGE_VERSION ${VERSION})
set(${PACKAGE}_VERSION ${VERSION})
string(REPLACE "." "-" GECODE_LIBRARY_VERSION "${VERSION}")
set(PACKAGE_STRING "${PACKAGE} ${VERSION}")
if (VERSION MATCHES "(.*)\\.(.*)\\.(.*)")
  math(EXPR GECODE_VERSION_NUMBER
    "${CMAKE_MATCH_1} * 100000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
endif ()

set(GECODE_DLL_USERPREFIX "")
set(GECODE_DLL_USERSUFFIX "")
set(GECODE_HAS_INT_VARS "/**/")
set(GECODE_HAS_SET_VARS "/**/")
set(GECODE_HAS_FLOAT_VARS "/**/")
set(GECODE_STATIC_LIBS 1)

check_cxx_compiler_flag(-fvisibility=hidden HAVE_VISIBILITY_HIDDEN_FLAG)
if (HAVE_VISIBILITY_HIDDEN_FLAG)
  set(GECODE_GCC_HAS_CLASS_VISIBILITY "/**/")
endif ()

if (WIN32)
  set(GECODE_THREADS_WINDOWS 1)
else ()
  set(GECODE_THREADS_PTHREADS 1)
endif ()

# Don't use Qt if GECODE_USE_QT is set to FALSE.
if (NOT DEFINED GECODE_USE_QT OR GECODE_USE_QT)
  find_package(Qt5 QUIET COMPONENTS Core Gui Widgets PrintSupport)
  if (Qt5_FOUND)
     set(GECODE_HAS_QT "/**/")
     set(GECODE_HAS_GIST "/**/")
     set(EXTRA_LIBS gist)
     set(CMAKE_AUTOMOC TRUE)
  else()
    find_package(Qt4)
    if (QT4_FOUND)
      set(GECODE_HAS_QT "/**/")
      set(GECODE_HAS_GIST "/**/")
      set(EXTRA_LIBS gist)
      set(CMAKE_AUTOMOC TRUE)
      include(${QT_USE_FILE})
    endif()
  endif()
endif ()

include(CheckSymbolExists)
check_symbol_exists(getpagesize unistd.h HAVE_GETPAGESIZE)
check_symbol_exists(mmap sys/mman.h HAVE_MMAP)

# Checks for header files.
include(CheckIncludeFiles)
foreach (header inttypes.h memory.h stdint.h stdlib.h strings.h string.h
                sys/param.h sys/stat.h sys/time.h sys/types.h unistd.h)
  string(TOUPPER HAVE_${header} var)
  string(REGEX REPLACE "\\.|/" "_" var ${var})
  check_include_files(${header} ${var})
endforeach ()
check_include_files(stdio.h STDC_HEADERS)
if (HAVE_SYS_TIME_H)
  set(GECODE_USE_GETTIMEOFDAY 1)
else ()
  set(GECODE_USE_CLOCK 1)
endif ()
if (HAVE_UNISTD_H)
  set(GECODE_HAS_UNISTD_H 1)
endif ()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
   #include <ext/hash_map>
   int main() {}" HAVE_EXT_HASH_MAP)
if (HAVE_EXT_HASH_MAP)
  set(GECODE_HAS_GNU_HASH_MAP "/**/")
endif ()

include(CheckTypeSize)
check_type_size(int SIZEOF_INT)

# Check for inline.
include(CheckCSourceCompiles)
check_c_source_compiles("
  inline __attribute__ ((__always_inline__)) void foo(void) {}
  int main() {}" HAVE_ALWAYS_INLINE)
set(forceinline inline)
if (HAVE_ALWAYS_INLINE)
  set(forceinline "inline __attribute__ ((__always_inline__))")
endif ()

check_c_source_compiles("
  int main() { return __builtin_ffsl(0); }" HAVE_BUILTIN_FFSL)
if (HAVE_BUILTIN_FFSL)
  set(GECODE_HAS_BUILTIN_FFSL "/**/")
endif ()

# Process config.hpp using autoconf rules.
list(LENGTH CONFIG length)
math(EXPR length "${length} - 1")
foreach (i RANGE ${length})
  list(GET CONFIG ${i} line)
  if (line MATCHES "^#( *)undef (.*)\n")
    set(space "${CMAKE_MATCH_1}")
    set(var ${CMAKE_MATCH_2})
    if (NOT DEFINED ${var} OR (var MATCHES "HAVE_.*" AND NOT ${var}))
      set(line "/* #${space}undef ${var} */\n")
    else ()
      if ("${${var}}" STREQUAL "/**/" OR "${var}" STREQUAL "GECODE_VERSION_NUMBER" OR
          "${var}" STREQUAL "forceinline" OR var MATCHES "SIZEOF_.*")
        set(value ${${var}})
      elseif (NOT (var MATCHES ^HAVE OR ${var} EQUAL 0 OR ${var} EQUAL 1))
        set(value \"${${var}}\")
      elseif (${var})
        set(value 1)
      else ()
        set(value 0)
      endif ()
      set(line "#${space}define ${var} ${value}\n")
    endif ()
  endif ()
  string(REPLACE "<semi>" ";" line "${line}")
  set(CONFIG_OUT "${CONFIG_OUT}${line}")
endforeach ()
file(WRITE ${GECODE_BINARY_DIR}/gecode/support/config.hpp.new
"/* gecode/support/config.hpp.  Generated from config.hpp.in by configure.  */
/* gecode/support/config.hpp.in.  Generated from configure.ac by autoheader.  */

/* Disable autolink because all the dependencies are handled by CMake. */
#define GECODE_BUILD_SUPPORT
#define GECODE_BUILD_KERNEL
#define GECODE_BUILD_SEARCH
#define GECODE_BUILD_INT
#define GECODE_BUILD_SET
#define GECODE_BUILD_FLOAT
#define GECODE_BUILD_MINIMODEL
#define GECODE_BUILD_FLATZINC
#define GECODE_BUILD_DRIVER
#define GECODE_BUILD_GIST

${CONFIG_OUT}")

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${GECODE_BINARY_DIR}/gecode/support/config.hpp.new
	${GECODE_BINARY_DIR}/gecode/support/config.hpp)

if(NOT DEFINED BIN_DIR)
   set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)

if(NOT DEFINED LIB_DIR)
   set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

# Expands a value substituting variables and appends the result to ${var}.
function (expand var value)
  if (value MATCHES "\\$\\(([^:]+)(.*)\\)")
    # Perform substitution.
    set(pattern ${CMAKE_MATCH_2})
    set(items ${${CMAKE_MATCH_1}})
    if (pattern MATCHES ":%=([^%]*)%([^%]*)")
      set(values )
      foreach (item ${items})
        set(values ${values} ${CMAKE_MATCH_1}${item}${CMAKE_MATCH_2})
      endforeach ()
    else ()
      set(values ${items})
    endif ()
  else ()
    set(values ${value})
  endif ()
  set(${var} ${${var}} ${values} PARENT_SCOPE)
endfunction ()

# Parse Makefile.in extracting variables.
file(READ Makefile.in text)
string(REPLACE "\\\n" "" text "${text}")
string(REGEX REPLACE "#[^\n]*\n" "" text "${text}")
string(REGEX MATCHALL "[^\n]+" lines "${text}")
foreach (line ${lines})
  if (line MATCHES "([^ \t]+)[ \t]*=[ \t]*(.*)")
    set(var ${CMAKE_MATCH_1})
    set(${var} )
    string(REGEX MATCHALL "[^ \t]+" items "${CMAKE_MATCH_2}")
    foreach (item ${items})
      expand(${var} ${item})
    endforeach ()
  endif ()
endforeach ()

foreach (lib support kernel search int set float
             minimodel driver flatzinc ${EXTRA_LIBS})
  if (lib STREQUAL "minimodel")
    set(libupper MM)
  else ()
    string(TOUPPER ${lib} libupper)
  endif ()
  if (${libupper}SRC)
    set(sources )
    foreach (src ${${libupper}SRC} ${${libupper}_GENSRC})
      if ((src STREQUAL "gecode/support/thread/pthreads.cpp" AND MSVC) OR
           src STREQUAL "gecode/float/rounding.cpp")
        # ignore empty source files to prevent linker warnings
      else ()
        set(sources ${sources} ${src})
      endif ()
    endforeach ()
    add_library(gecode${lib} ${sources} ${${libupper}HDR})
    install(TARGETS gecode${lib}
	    RUNTIME DESTINATION ${BIN_DIR}
	    LIBRARY DESTINATION ${LIB_DIR}
	    ARCHIVE DESTINATION ${LIB_DIR})
  endif ()
endforeach ()

find_package(Threads)
target_link_libraries(gecodesupport ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(gecodekernel gecodesupport)
target_link_libraries(gecodesearch gecodekernel)
target_link_libraries(gecodeint gecodekernel)
target_link_libraries(gecodeset gecodeint)
target_link_libraries(gecodefloat gecodeint)
target_link_libraries(gecodeminimodel gecodeint gecodeset gecodesearch)
target_link_libraries(gecodedriver gecodeint)
target_link_libraries(gecodeflatzinc gecodeminimodel gecodedriver)

if (GECODE_HAS_QT)
  if (Qt5_FOUND)
    qt5_use_modules(gecodegist Widgets Gui PrintSupport)
  else()
    target_link_libraries(gecodegist ${QT_LIBRARIES})
    target_link_libraries(gecodeflatzinc gecodegist ${QT_LIBRARIES})
  endif()
endif ()

if (FLOATSRC)
  target_link_libraries(gecodefloat gecodekernel)
  target_link_libraries(gecodeminimodel gecodefloat)
endif ()
