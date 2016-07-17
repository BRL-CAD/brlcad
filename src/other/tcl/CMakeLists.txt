#                     C M A K E L I S T S . T X T
# TCL
#
# Copyright (c) 2010 United States Government as represented by
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

# *******************************************************************
# ***                    Tcl CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  if("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
    CMAKE_POLICY(SET CMP0048 OLD)
  endif("${CMAKE_VERSION}" VERSION_GREATER 2.99)
  if("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
    CMAKE_POLICY(SET CMP0053 OLD)
    CMAKE_POLICY(SET CMP0054 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)


# Set CMake project name
project(TCL)

# build shared libs by default
if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build shared libraries" ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

# build static libs by default
if(NOT DEFINED BUILD_STATIC_LIBS)
  option(BUILD_STATIC_LIBS "Build shared libraries" ON)
endif(NOT DEFINED BUILD_STATIC_LIBS)

# version numbers
set(TCL_VERSION_MAJOR 8)
set(TCL_VERSION_MINOR 6)
set(TCL_VERSION_PATCH 5)
set(TCL_VERSION "${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}.${TCL_VERSION_PATCH}")

# For Windows, we need the Resource Compiler language
if(WIN32)
  enable_language(RC)
endif(WIN32)

#----------------------------------------------------------------------------
# The location in which to install Tcl.  Only do this if CMAKE_INSTALL_PREFIX
# hasn't been set already, to try and allow parent builds (if any) some control.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(NOT WIN32)
    if("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
      set(CMAKE_INSTALL_PREFIX "/usr/local/tcldev-${TCL_VERSION}")
    else("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
      set(CMAKE_INSTALL_PREFIX "/usr")
    endif("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  else(NOT WIN32)
    set(CMAKE_INSTALL_PREFIX "C:/Program Files/Tcl ${TCL_VERSION}")
  endif(NOT WIN32)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "TCL install prefix" FORCE)
  set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

#-----------------------------------------------------------------------------
# Set CMake module path
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${TCL_SOURCE_DIR}/CMake")
set(TCL_CMAKE_DIR "${TCL_SOURCE_DIR}/CMake")

#-----------------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in the build
# directory AND install directory. Documentation of these options is available
# at http://www.cmake.org/Wiki/CMake_RPATH_handling

include("${TCL_CMAKE_DIR}/RPath_Setup.cmake")

#---------------------------------------------------------------------
# Define relative install locations and output directories.  Don't set
# these if they have already been set by some other means (like a
# higher level CMakeLists.txt file including this one).

include("${TCL_CMAKE_DIR}/Path_Setup.cmake")


#----------------------------------------------------------------------------
# Define some standard configuration settings passed to all Tcl build targets
add_definitions(-DPACKAGE_NAME="tcl")
add_definitions(-DPACKAGE_TARNAME="tcl")
add_definitions(-DPACKAGE_VERSION="${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
add_definitions(-DPACKAGE_STRING="tcl ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}")
add_definitions(-DPACKAGE_BUGREPORT="")

# We're building Tcl
add_definitions(-DBUILD_tcl)

# First, get some standard settings out of the way
# Assume we have STDC_HEADERS until we meet a situation where we don't
add_definitions(-DSTDC_HEADERS=1)
# Get the SHLIB extension from CMake
add_definitions(-DTCL_SHLIB_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}")


#----------------------------------------------------------------------------
# Define some platform specific flags - ideally we'd configure test for all
# of these, but until then just do what's needed.

# Windows specific flags
if(MSVC)
  add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP -Ot -Oi -fp:strict -Gs -GS -GL -MD)
  add_definitions(-DTCL_PIPE_DLL="tclpip${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}.dll")
  add_definitions(-Dinline=__inline -DBUILD_tcl)
endif(MSVC)

if(MINGW)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP)
  add_definitions(-DTCL_PIPE_DLL="tclpip${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}.dll")
  add_definitions(-Dinline=__inline -DBUILD_tcl)
  remove_definitions(-w)
  add_definitions(-DHAVE_NO_SEH -DEXCEPTION_DISPOSITION=int)
endif(MINGW)

if(APPLE)
  add_definitions(-DMAC_OSX_TCL=1)
endif(APPLE)

#------------------------------------------------------------------------
# Check if the compiler understands -pipe.  If so, use it.
# It makes compiling go faster.  (This is only a performance feature.)
#------------------------------------------------------------------------
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-pipe PIPE_COMPILER_FLAG)
if(PIPE_COMPILER_FLAG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
endif(PIPE_COMPILER_FLAG)


#--------------------------------------------------------------------
# Header checks.
#--------------------------------------------------------------------
include(CMakeParseArguments)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)

function(Tcl_Check_Include_File filename)
  cmake_parse_arguments(OPT "USABILITY" "TESTSRC;COMPATSRC;DEFVAR" "" ${ARGN})
  string(REPLACE "." "_" var "${filename}")
  string(REPLACE "/" "_" var "${var}")
  string(TOUPPER "${var}" var)
  message("var: ${var}")
  CHECK_INCLUDE_FILES(${filename} HAVE_${var})
  if(HAVE_${var})
    if(OPT_USABILITY)
      # Usability test requested - we're not done yet.
      if(OPT_TESTSRC)
	CHECK_C_SOURCE_COMPILES("${${OPT_TESTSRC}}" ${var}_USABLE)
      else(OPT_TESTSRC)
	CHECK_C_SOURCE_COMPILES("#include <${filename}>\nint main() {return 0;}" ${var}_USABLE)
      endif(OPT_TESTSRC)
      if(${var}_USABLE)
	if(OPT_DEFVAR)
	  set(${OPT_DEFVAR} 1 PARENT_SCOPE)
	  add_definitions(-D${OPT_DEFVAR}=1)
	else(OPT_DEFVAR)
	  set(HAVE_${var} 1 PARENT_SCOPE)
	  add_definitions(-DHAVE_${var}=1)
	endif(OPT_DEFVAR)
      else(${var}_USABLE)
	add_definitions(-DNO_${var}=1)
	if(OPT_COMPAT_SRC)
	  set(COMPAT_SRCS "${COMPAT_SRCS}" ${OPT_COMPAT_SRC} PARENT_SCOPE)
	endif(OPT_COMPAT_SRC)
      endif(${var}_USABLE)
    else(OPT_USABILITY)
      if(OPT_DEFVAR)
	set(${OPT_DEFVAR} 1 PARENT_SCOPE)
	add_definitions(-D${OPT_DEFVAR}=1)
      else(OPT_DEFVAR)
	set(HAVE_${var} 1 PARENT_SCOPE)
	add_definitions(-DHAVE_${var}=1)
      endif(OPT_DEFVAR)
    endif(OPT_USABILITY)
  else(HAVE_${var})
    if(OPT_COMPATSRC)
      set(COMPAT_SRCS "${COMPAT_SRCS}" ${OPT_COMPATSRC} PARENT_SCOPE)
    endif(OPT_COMPATSRC)
  endif(HAVE_${var})
endfunction(Tcl_Check_Include_File)



set(DIRENT_TEST_SRCS "
#include <sys/types.h>
#include <dirent.h>
int main () {
#ifndef _POSIX_SOURCE
#   ifdef __Lynx__
        /*
         * Generate compilation error to make the test fail:  Lynx headers
         * are only valid if really in the POSIX environment.
         */

        missing_procedure();
#   endif
#endif
DIR *d;
struct dirent *entryPtr;
char *p;
d = opendir(\"foobar\");
entryPtr = readdir(d);
p = entryPtr->d_name;
closedir(d);
return 0;
}
")
set(LANG_TEST_SRCS "
#include <langinfo.h>
int main () {
nl_langinfo(CODESET);
return 0;
}
")
# Check for strtol, strtoul and strtod in stdlib.h
# before accepting it.
set(STDLIB_TEST_SRCS "
#include <stdlib.h>
int main () {
long l = strtol(\"123\", NULL, 10);
unsigned long ul = strtoul(\"123\", NULL, 10);
double d = strtod(\"123.456\", NULL);
return 0;
}
")
# Check for strstr and strerror in string.h
# before accepting it.
set(STRING_TEST_SRCS "
asdfas
#include <string.h>
int main () {
char *str = \"test0 test1 test2\";
char *ss = strstr(str,\"test1\");
char *es = strerror(1);
return 0;
}
")
Tcl_Check_Include_File(AvailabilityMacros.h)
Tcl_Check_Include_File(copyfile.h)
Tcl_Check_Include_File(dirent.h USABILITY TESTSRC DIRENT_TEST_SRCS)
Tcl_Check_Include_File(dlfcn.h USABILITY)
Tcl_Check_Include_File(float.h USABILITY)
Tcl_Check_Include_File(langinfo.h USABILITY TESTSRC LANG_TEST_SRCS DEFVAR HAVE_LANGINFO)
Tcl_Check_Include_File(limits.h USABILITY)
Tcl_Check_Include_File(net/errno.h)
Tcl_Check_Include_File(stdlib.h USABILITY TESTSRC STDLIB_TEST_SRCS)
Tcl_Check_Include_File(string.h USABILITY TESTSRC STRING_TEST_SRCS)
Tcl_Check_Include_File(sys/filio.h)
Tcl_Check_Include_File(sys/ioctl.h)
Tcl_Check_Include_File(sys/modem.h)
Tcl_Check_Include_File(sys/param.h USABILITY)
Tcl_Check_Include_File(sys/sdt.h)
Tcl_Check_Include_File(sys/time.h)
Tcl_Check_Include_File(sys/wait.h USABILITY)
Tcl_Check_Include_File(termios.h)
Tcl_Check_Include_File(values.h USABILITY)


#--------------------------------------------------------------------
# Function checks.
#--------------------------------------------------------------------
include(CMakeParseArguments)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

function(TCL_CHECK_FUNCTION_EXISTS fname)
  cmake_parse_arguments(OPT "" "COMPAT_SRC;DEFVAR" "" ${ARGN})
  string(TOUPPER "${var}" fname)
  message("fvar: ${var}")
  CHECK_FUNCTION_EXISTS(${fname} HAVE_${var})
  if(HAVE_${var})
    if(OPT_DEFVAR)
      set(${OPT_DEFVAR} 1 PARENT_SCOPE)
      add_definitions(-D${OPT_DEFVAR}=1)
    else(OPT_DEFVAR)
      set(HAVE_${var} 1 PARENT_SCOPE)
      add_definitions(-DHAVE_${var}=1)
    endif(OPT_DEFVAR)
  else(HAVE_${var})
    if(OPT_NOFLAG)
      add_definitions(-DNO_${var}=1)
    endif(OPT_NOFLAG)
    if(OPT_COMPAT_SRC)
      set(COMPAT_SRCS "${COMPAT_SRCS}" ${OPT_COMPAT_SRC} PARENT_SCOPE)
    endif(OPT_COMPAT_SRC)
  endif(HAVE_${var})
endfunction(TCL_CHECK_FUNCTION_EXISTS)





#--------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/tcl.cmake")

#------------------------------------------------------------------------
# Threads support
#------------------------------------------------------------------------
find_package(Threads)
if(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")
  add_definitions(-DTCL_THREADS=1)
  add_definitions(-DUSE_THREAD_ALLOC=1)
  add_definitions(-D_REENTRANT=1)
  add_definitions(-D_THREAD_SAFE=1)

endif(NOT ${CMAKE_THREAD_LIBS_INIT} STREQUAL "")

#------------------------------------------------------------------------
# Embedded configuration information, encoding to use for the values, TIP #59
#------------------------------------------------------------------------
SC_TCL_CFG_ENCODING()

#--------------------------------------------------------------------
# Look for libraries that we will need when compiling the Tcl shell
#--------------------------------------------------------------------
SC_TCL_LINK_LIBS()

# Add the Threading library, if we're using it
SET(TCL_LINK_LIBS ${TCL_LINK_LIBS} ${TCL_THREADS_LIB})

# Same for the math library
SET(TCL_LINK_LIBS ${TCL_LINK_LIBS} ${M_LIBRARY})

#--------------------------------------------------------------------
# Handle zlib
#--------------------------------------------------------------------
find_package(ZLIB)
# We're going to have zlib one way or the other,
add_definitions(-DHAVE_ZLIB=1)

# TODO - look over SC_ENABLE_SHARED, SC_CONFIG_CFLAGS and SC_ENABLE_SYMBOLS
# need to decide how much of that logic is needed.  Clearly we need SOME
# of it.

# Load and Unload
OPTION(TCL_ENABLE_LOAD "Enable dynamic loading" ON)
IF(TCL_ENABLE_LOAD)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_DL_LIBRARY)
  IF(HAVE_DL_LIBRARY)
    set(DL_LIBRARY "dl")
  ENDIF(HAVE_DL_LIBRARY)
ENDIF(TCL_ENABLE_LOAD)

OPTION(TCL_ENABLE_DLL_UNLOADING "Allow unloading of shared libraries" ON)
IF(TCL_ENABLE_DLL_UNLOADING)
  add_definitions(-DTCL_UNLOAD_DLLS=1)
ENDIF(TCL_ENABLE_DLL_UNLOADING)

# CoreFoundation check.  TODO - check for 64-bit stuff at some point
IF(APPLE)
  OPTION(TCL_ENABLE_COREFOUNDATION "Use CoreFoundation" ON)
  IF(TCL_ENABLE_COREFOUNDATION)
    INCLUDE(CMakeFindFrameworks)
    CMAKE_FIND_FRAMEWORKS(CoreFoundation)
    IF(CoreFoundation_FRAMEWORKS)
      SET(CoreFoundation_LIBRARIES "-framework CoreFoundation")
      add_definitions(-DHAVE_COREFOUNDATION=1)
    ENDIF(CoreFoundation_FRAMEWORKS)
  ENDIF(TCL_ENABLE_COREFOUNDATION)
ENDIF(APPLE)

#----------------------------------------------------------------------------
# Tom's math library - should MP_PREC be an option?
#----------------------------------------------------------------------------
OPTION(TCL_TOMMATH "Enable Tom's Math Library" ON)
IF(TCL_TOMMATH)
  add_definitions(-DTCL_TOMMATH=1)
  add_definitions(-DMP_PREC=4)
ENDIF(TCL_TOMMATH)

#----------------------------------------------------------------------------
# 64-bit support - unlike TEA, we enable by default if the platform looks
# like it's 64-bit.  Can be overridden by specifying CMAKE_WORD_SIZE=32BIT
#----------------------------------------------------------------------------
SC_TCL_64BIT_FLAGS()

#----------------------------------------------------------------------------
# Add options for optimized and debug compile options - replaces 
# SC_ENABLE_SYMBOLS
#----------------------------------------------------------------------------
OPTION(TCL_OPTIMIZED "Built optimized Tcl" ON)
IF(TCL_OPTIMIZED)
  add_definitions(-DTCL_CFG_OPTIMIZED=1)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O2")
ENDIF(TCL_OPTIMIZED)

OPTION(TCL_DEBUG "Enable Debugging" ON)
IF(TCL_DEBUG)
  add_definitions(-DTCL_CFG_DEBUG=1)
ENDIF(TCL_DEBUG)

OPTION(TCL_COMPILE_DEBUG "Bytecode debugging" OFF)
IF(TCL_COMPILE_DEBUG)
  add_definitions(-DTCL_COMPILE_DEBUG=1)
else(TCL_COMPILE_DEBUG)
  add_definitions(-DTCL_COMPILE_DEBUG=0)
ENDIF(TCL_COMPILE_DEBUG)

OPTION(TCL_COMPILE_STATS "Bytecode statistics" OFF)
IF(TCL_COMPILE_STATS)
  add_definitions(-DTCL_COMPILE_STATS=1)
ENDIF(TCL_COMPILE_STATS)

add_definitions(-DUSE_TCLALLOC=0)
add_definitions(-DYYENABLE_NLS=0)

#--------------------------------------------------------------------
#       Check endianness because we can optimize comparisons of
#       Tcl_UniChar strings to memcmp on big-endian systems.
#--------------------------------------------------------------------
# Test endianness 
IF(NOT WIN32)
  INCLUDE(TestBigEndian)
  TEST_BIG_ENDIAN(WORDS_BIGENDIAN)
  IF(WORDS_BIGENDIAN)
    add_definitions(-DWORDS_BIGENDIAN=1)
  ENDIF(WORDS_BIGENDIAN)
ENDIF(NOT WIN32)

#--------------------------------------------------------------------
#       Supply substitutes for missing POSIX library procedures, or
#       set flags so Tcl uses alternate procedures.
#--------------------------------------------------------------------
TCL_CHECK_FUNCTION_EXISTS(getcwd HAVE_GETCWD)
IF(NOT HAVE_GETCWD)
  TCL_CHECK_FUNCTION_EXISTS(getwd HAVE_GETWD)
  IF(NOT HAVE_GETWD)
    add_definitions(-DNO_GETWD=1)
  ELSE(NOT HAVE_GETWD)
    add_definitions(-DUSEGETWD=1)
  ENDIF(NOT HAVE_GETWD)
ENDIF(NOT HAVE_GETCWD)
TCL_CHECK_FUNCTION_EXISTS(hypot HAVE_HYPOT)
IF(NOT HAVE_HYPOT)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/hypot.c)
ENDIF()
TCL_CHECK_FUNCTION_EXISTS(mkstemp HAVE_MKSTEMP)
IF(NOT HAVE_MKSTEMP)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/mkstemp.c)
ENDIF(NOT HAVE_MKSTEMP)
TCL_CHECK_FUNCTION_EXISTS(opendir HAVE_OPENDIR)
IF(NOT HAVE_OPENDIR)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/opendir.c)
ENDIF(NOT HAVE_OPENDIR)
TCL_CHECK_FUNCTION_EXISTS(strtol HAVE_STRTOL)
IF(NOT HAVE_STRTOL)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/strtol.c)
ENDIF(NOT HAVE_STRTOL)
TCL_CHECK_FUNCTION_EXISTS(waitpid HAVE_WAITPID)
IF(NOT HAVE_WAITPID)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/waitpid.c)
ENDIF(NOT HAVE_WAITPID)
CHECK_FUNCTION_EXISTS(strerror HAVE_STRERROR)
IF(NOT HAVE_STRERROR)
  add_definitions(-DNO_STRERROR=1)
ENDIF(NOT HAVE_STRERROR)
CHECK_FUNCTION_EXISTS(wait3 HAVE_WAIT3)
IF(NOT HAVE_WAIT3)
  add_definitions(-DNO_WAIT3=1)
ENDIF(NOT HAVE_WAIT3)
CHECK_FUNCTION_EXISTS(uname HAVE_UNAME)
IF(NOT HAVE_UNAME)
  add_definitions(-DNO_UNAME=1)
ENDIF(NOT HAVE_UNAME)

# Check for early Darwin version here - realpath
# is not threadsafe prior to Darwin 7
CHECK_FUNCTION_EXISTS(realpath HAVE_REALPATH)
IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND TCL_THREADS AND HAVE_REALPATH)
  STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
  IF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
    MESSAGE("realpath is not threadsafe in Darwin versions prior to 7, disabling")
    SET(HAVE_REALPATH)
  ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND TCL_THREADS AND HAVE_REALPATH)
IF(NOT HAVE_REALPATH)
  add_definitions(-DNO_REALPATH=1)
ENDIF(NOT HAVE_REALPATH)

SC_TCL_IPV6()

#-------------------------------------------------------------------- 
#       Look for thread-safe variants of some library functions.
#--------------------------------------------------------------------

IF(TCL_THREADS)

  SC_TCL_GETPWUID_R()

  SC_TCL_GETPWNAM_R()

  SC_TCL_GETGRGID_R()

  SC_TCL_GETGRNAM_R()

  IF(NOT HAVE_GETHOSTBYNAME)
    CHECK_LIBRARY_EXISTS(GHBN lnsl "" gethostbyname)
    SET(HAVE_GETHOSTBYNAME ${GHBN})
  ENDIF(NOT HAVE_GETHOSTBYNAME)

  SC_TCL_GETHOSTBYADDR_R()
  SC_TCL_GETHOSTBYNAME_R()

  IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
    STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
    IF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYNAME=1)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYADDR=1)
    ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
  ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

  IF(${CMAKE_SYSTEM_NAME} MATCHES "^HP-UX$")
    STRING(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
    IF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 10)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYNAME=1)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYADDR=1)
    ENDIF (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 10)
  ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^HP-UX$")

ENDIF(TCL_THREADS)

#---------------------------------------------------------------------------
#       Determine which interface to use to talk to the serial port.
#       Note that #include lines must begin in leftmost column for
#       some compilers to recognize them as preprocessor directives.
#---------------------------------------------------------------------------
SC_SERIAL_PORT()

#---------------------------------------------------------------------------
# TODO - Do we need the sys/select.h check these days?
#---------------------------------------------------------------------------

#------------------------------------------------------------------------------
#       Find out all about time handling differences.
#------------------------------------------------------------------------------
SC_TIME_HANDLER()

#---------------------------------------------------------------------------
# Check for stat structure files and blkcnt_t
#---------------------------------------------------------------------------

TCL_CHECK_STRUCT_HAS_MEMBER("struct stat" st_blocks sys/stat.h	STRUCT_HAS_ST_BLOCKS)
TCL_CHECK_STRUCT_HAS_MEMBER("struct stat" st_blksize sys/stat.h STRUCT_HAS_ST_BLKSIZE)
CHECK_TYPE_SIZE(blkcnt_t HAVE_BLKCNT_T)
IF(HAVE_BLKCNT_T)
  add_definitions(-DHAVE_BLKCNT_T=1)
ENDIF(HAVE_BLKCNT_T)
TCL_CHECK_FUNCTION_EXISTS(fstatfs, HAVE_FSTATFS)
IF(NOT HAVE_FSTATFS)
  add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_FSTATFS)

#---------------------------------------------------------------------------
# GNU manual lists AC_FUNC_MEMCMP as obsolete - just check for memcmp
#---------------------------------------------------------------------------
TCL_CHECK_FUNCTION_EXISTS(memcmp HAVE_MEMCMP)
IF(NOT HAVE_MEMCMP)
  SET(COMPAT_SRCS ${COMPAT_SRCS} compat/memcmp.c)
ENDIF(NOT HAVE_MEMCMP)

#---------------------------------------------------------------------------
# Check for memmove
#---------------------------------------------------------------------------
# need to revisit this, not finding memmove
#TCL_CHECK_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)
#IF(NOT HAVE_MEMMOVE)
#	add_definitions(-DNO_MEMMOVE=1)
#	TCL_CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
#	IF(NOT HAVE_STRING_H)
#		add_definitions(-DNO_STRING_H=1)
#	ENDIF(NOT HAVE_STRING_H)
#ENDIF(NOT HAVE_MEMMOVE)

#------------------------------------------------------------------------------
# Test for broken functions.  Not clear how many of these are still needed.
#------------------------------------------------------------------------------
SC_TCL_CHECK_BROKEN_FUNC(
  strstr "
  extern int strstr();
  exit(strstr(\"\\\0test\", \"test\") ? 1 : 0);"
  )


SC_TCL_CHECK_BROKEN_FUNC(
  strtoul "
  extern int strtoul();
  char *term, *string = \"0\";
  exit(strtoul(string,&term,0) != 0 || term != string+1);"
  )


SC_TCL_CHECK_BROKEN_FUNC(
  strtod "
  extern double strtod();
  char *term, *string = \" +69\";
  exit(strtod(string,&term) != 69 || term != string+4);"
  )



#TODO SC_BUGGY_STRTOD - Solaris 2.4 only?  Do we still care?

#--------------------------------------------------------------------
#       Check for various typedefs and provide substitutes if
#       they don't exist.
#--------------------------------------------------------------------
TCL_CHECK_TYPE_SIZE(mode_t MODE)
TCL_CHECK_TYPE_SIZE(pid_t PID)
TCL_CHECK_TYPE_SIZE(size_t SIZE)
TCL_CHECK_TYPE_SIZE(uid_t UID)
# The following tests may need to be more elaborate to function properly
TCL_CHECK_TYPE_SIZE(socklen_t SOCKLEN sys/types.h sys/socket.h)
TCL_CHECK_TYPE_SIZE(intptr_t INTPTR)
TCL_CHECK_TYPE_SIZE(uintptr_t UINTPTR)

IF(NOT HAVE_OPENDIR)
  add_definitions(-DUSE_DIRENT2_H=1)
ENDIF(NOT HAVE_OPENDIR)

# The union wait test was for HP-UX - do we still need this?
SET(UNION_WAIT_SRC "
#include <sys/types.h>
#include <sys/wait.h>
int
main ()
{

union wait x;
WIFEXITED(x);           /* Generates compiler error if WIFEXITED
* uses an int. */
return 0;
}
")
if(NOT DEFINED HAVE_UNION_WAIT)
  CHECK_C_SOURCE_RUNS("${UNION_WAIT_SRC}" HAVE_UNION_WAIT)
endif(NOT DEFINED HAVE_UNION_WAIT)
IF(NOT HAVE_UNION_WAIT)
  add_definitions(-DNO_UNION_WAIT=1)
ENDIF(NOT HAVE_UNION_WAIT)

#----------------------------------------------------------------------------
# Check for strncasecmp
#----------------------------------------------------------------------------
CHECK_FUNCTION_EXISTS(strncasecmp HAVE_STRNCASECMP)
IF(NOT HAVE_STRNCASECMP)
  CHECK_LIBRARY_EXISTS(socket strncasecmp "" HAVE_SOCKET_STRNCASECMP)
  CHECK_LIBRARY_EXISTS(inet strncasecmp "" HAVE_INET_STRNCASECMP)
  IF(NOT HAVE_SOCKET_STRNCASECMP AND NOT HAVE_INET_STRNCASECMP)
    SET(COMPAT_SRCS ${COMPAT_SRCS} compat/strncasecmp.c)
  ENDIF(NOT HAVE_SOCKET_STRNCASECMP AND NOT HAVE_INET_STRNCASECMP)
ENDIF(NOT HAVE_STRNCASECMP)

#----------------------------------------------------------------------------
# gettimeofday issues
#----------------------------------------------------------------------------
TCL_CHECK_FUNCTION_EXISTS(BSDgettimeofday HAVE_BSDGETTIMEOFDAY)
IF(NOT HAVE_BSDGETTIMEOFDAY)
  CHECK_FUNCTION_EXISTS(gettimeofday HAVE_GETTIMEOFDAY)
  IF(NOT HAVE_GETTIMEOFDAY)
    add_definitions(-DNO_GETTOD=1)
  ENDIF(NOT HAVE_GETTIMEOFDAY)
ENDIF(NOT HAVE_BSDGETTIMEOFDAY)
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(gettimeofday "sys/time.h" IS_GETTOD_DECLARED)
IF(NOT IS_GETTOD_DECLARED)
  add_definitions(-DGETTOD_NOT_DECLARED=1)
ENDIF(NOT IS_GETTOD_DECLARED)


#----------------------------------------------------------------------------
# signed/unsigned char - does the Tcl code still need this? See
# http://lists.gnu.org/archive/html/autoconf/2008-06/msg00054.html
#----------------------------------------------------------------------------
SET(unsigned_char_srcs "
int
main ()
{
static int test_array [1 - 2 * !(((char) -1) < 0)];
test_array [0] = 0;
return 0;
}
")
if(NOT DEFINED CHAR_IS_UNSIGNED)
  CHECK_C_SOURCE_RUNS("${unsigned_char_srcs}" CHAR_IS_UNSIGNED)
endif(NOT DEFINED CHAR_IS_UNSIGNED)
IF(CHAR_IS_UNSIGNED)
  add_definitions(-D__CHAR_UNSIGNED__)
ENDIF(CHAR_IS_UNSIGNED)
SET(signed_char_srcs "
int
main ()
{
signed char *p;
p = 0;
return 0;
}
")
CHECK_C_SOURCE_COMPILES("${signed_char_srcs}" HAVE_SIGNED_CHAR)
IF(HAVE_SIGNED_CHAR)
  add_definitions(-DHAVE_SIGNED_CHAR=1)
ENDIF(HAVE_SIGNED_CHAR)

#--------------------------------------------------------------------
# Check for support of nl_langinfo function
#--------------------------------------------------------------------
SC_ENABLE_LANGINFO()

#--------------------------------------------------------------------
# Check for chflags and mkstemps
#--------------------------------------------------------------------
TCL_CHECK_FUNCTION_EXISTS(chflags HAVE_CHFLAGS)
TCL_CHECK_FUNCTION_EXISTS(mkstemps HAVE_MKSTEMPS)


#--------------------------------------------------------------------
# Check for support of isnan() function or macro
#--------------------------------------------------------------------
SET(isnan_srcs "
#include <math.h>
int main () {
isnan(0.0); /* Generates an error if isnan is missing */
return 0;
}
")
if(NOT DEFINED USABLE_ISNAN)
  CHECK_C_SOURCE_RUNS("${isnan_srcs}" USABLE_ISNAN)
endif(NOT DEFINED USABLE_ISNAN)
IF(NOT USABLE_ISNAN)
  add_definitions(-DNO_ISNAN=1)
ENDIF(NOT USABLE_ISNAN)

#----------------------------------------------------------------------------
# Darwin specific API checks and defines
#--------------------------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
  TCL_CHECK_FUNCTION_EXISTS(getattrlist HAVE_GETATTRLIST)
  TCL_CHECK_INCLUDE_FILE(copyfile.h HAVE_COPYFILE_H)
  TCL_CHECK_FUNCTION_EXISTS(copyfile HAVE_COPYFILE)
  IF(${CoreFoundation_LIBRARIES})
    TCL_CHECK_INCLUDE_FILE(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
    TCL_CHECK_FUNCTION_EXISTS(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
    TCL_CHECK_FUNCTION_EXISTS(pthread_atfork HAVE_PTHREAD_ATFORK)
  ENDIF(${CoreFoundation_LIBRARIES})
  add_definitions(-DUSE_VFORK=1)
  add_definitions(-DTCL_DEFAULT_ENCODING="utf-8")
  add_definitions(-DTCL_LOAD_FROM_MEMORY=1)
  add_definitions(-DTCL_WIDE_CLICKS=1)
  TCL_CHECK_INCLUDE_FILE_USABILITY(AvailabilityMacros.h HAVE_AVAILABILITYMACROS_H)
  IF(HAVE_AVAILABILITYMACROS_H)
    SET(WEAK_IMPORT_SRCS "
    #ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
    #if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
    #error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1020
    #endif
    #elif MAC_OS_X_VERSION_MIN_REQUIRED < 1020
    #error MAC_OS_X_VERSION_MIN_REQUIRED < 1020
    #endif
    int rand(void) __attribute__((weak_import));
    int main() {
    rand();
    return 0;
    }
    ")
    CHECK_C_SOURCE_COMPILES("${WEAK_IMPORT_SRCS}" WEAK_IMPORT_WORKING)
    IF(WEAK_IMPORT_WORKING)
      add_definitions(-DHAVE_WEAK_IMPORT=1)
    ENDIF(WEAK_IMPORT_WORKING)
    SET(SUSV3_SRCS "
    #ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
    #if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
    #error __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1050
    #endif
    #elif MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    #error MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    #endif
    #define _DARWIN_C_SOURCE 1
    #include <sys/cdefs.h>

    int main ()	{return 0;}
    ")
    CHECK_C_SOURCE_COMPILES("${SUSV3_SRCS}" SUSV3_WORKING)
    IF(SUSV3_WORKING)
      add_definitions(-D_DARWIN_C_SOURCE=1)
    ENDIF(SUSV3_WORKING)

  ENDIF(HAVE_AVAILABILITYMACROS_H)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

#--------------------------------------------------------------------
#       Check for support of fts functions (readdir replacement)
#--------------------------------------------------------------------
SET(FTS_SRCS "
#include <sys/param.h>
#include <sys/stat.h>
#include <fts.h>

int main () {
char*const p[2] = {\"/\", NULL};
FTS *f = fts_open(p, FTS_PHYSICAL|FTS_NOCHDIR|FTS_NOSTAT, NULL);
FTSENT *e = fts_read(f); fts_close(f);
return 0;
}
")
CHECK_C_SOURCE_COMPILES("${FTS_SRCS}" FTS_WORKING)
IF(FTS_WORKING)
  add_definitions(-DHAVE_FTS=1)
ENDIF(FTS_WORKING)

# TODO SC_BLOCKING_STYLE is for older systems, do we still need it?

#------------------------------------------------------------------------
#       Check whether the timezone data is supplied by the OS or has
#       to be installed by Tcl. The default is autodetection, but can
#       be overriden on the configure command line either way.
#------------------------------------------------------------------------
IF(TCL_TIMEZONE_DATA)
  IF(${TCL_TIMEZONE_DATA} STREQUAL "")
    SET(TCL_TIMEZONE_DATA "AUTO" CACHE STRING "Use Tcl's local timezone data")
  ENDIF(${TCL_TIMEZONE_DATA} STREQUAL "")
ELSE(TCL_TIMEZONE_DATA)
  SET(TCL_TIMEZONE_DATA "AUTO" CACHE STRING "Use Tcl's local timezone data")
ENDIF(TCL_TIMEZONE_DATA)
IF(${TCL_TIMEZONE_DATA} STREQUAL "AUTO")
  SET(LOCAL_TZ_DATA OFF)
  find_package(TIMEZONE)
  IF(NOT TIMEZONE_FOUND)
    SET(LOCAL_TZ_DATA ON)
  ENDIF(NOT TIMEZONE_FOUND)
ELSE(${TCL_TIMEZONE_DATA} STREQUAL "AUTO")
  SET(LOCAL_TZ_DATA ${TCL_TIMEZONE_DATA})
ENDIF(${TCL_TIMEZONE_DATA} STREQUAL "AUTO")

#--------------------------------------------------------------------
#       DTrace support
#--------------------------------------------------------------------
# TODO

#--------------------------------------------------------------------
#  Does the C stack grow upwards or downwards? Or cross-compiling?
#--------------------------------------------------------------------
SET(C_STACK_SRC "
int StackGrowsUp(int *parent) {
int here;
return (&here < parent);
}
int main (int argc, char *argv[]) {
int foo;
return StackGrowsUp(&foo);
}
")
if(NOT DEFINED STACK_GROWS_UP)
  CHECK_C_SOURCE_RUNS("${C_STACK_SRC}" STACK_GROWS_UP)
endif(NOT DEFINED STACK_GROWS_UP)
IF(STACK_GROWS_UP)
  add_definitions(-DTCL_STACK_GROWS_UP=1)
ENDIF(STACK_GROWS_UP)

TCL_CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
TCL_CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
TCL_CHECK_INCLUDE_FILE(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
  add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_SYS_FSTATFS_H)
TCL_CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
TCL_CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
TCL_CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
TCL_CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
TCL_CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
TCL_CHECK_INCLUDE_FILE(AvailabilityMacros.h HAVE_AVAILABILITYMACROS_H)
IF(HAVE_AVAILABILITYMACROS_H)
  add_definitions(-DHAVE_AVAILABILITY_MACROS_H)
ENDIF(HAVE_AVAILABILITYMACROS_H)

CHECK_COMPILER_SUPPORTS_HIDDEN_D()

TCL_CHECK_INCLUDE_FILE_USABILITY(sys/modem.h SYS_MODEM_H)


CHECK_FD_SET_IN_TYPES_D()

#--------------------------------------------------------------------
#       OSX Framework Configuration
#--------------------------------------------------------------------
# TODO

# If we collected any compat sources, pass the flag
IF(COMPAT_SRCS)
  add_definitions(-DUSE_COMPAT=1)
ENDIF(COMPAT_SRCS)

IF(APPLE)
  # Now that all the tests are done, configure the tclConfig.h file:
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unix/tclConfig.h.in ${TCL_BINARY_DIR}/include/tclConfig.h)
ENDIF(APPLE)

ADD_SUBDIRECTORY(library)

SET(TCL_GENERIC_SRCS
  generic/regcomp.c
  generic/regexec.c
  generic/regfree.c
  generic/regerror.c
  generic/tclAlloc.c
  generic/tclAssembly.c
  generic/tclAsync.c
  generic/tclBasic.c
  generic/tclBinary.c
  generic/tclCkalloc.c
  generic/tclClock.c
  generic/tclCmdAH.c
  generic/tclCmdIL.c
  generic/tclCmdMZ.c
  generic/tclCompCmds.c
  generic/tclCompCmdsGR.c
  generic/tclCompCmdsSZ.c
  generic/tclCompExpr.c
  generic/tclCompile.c
  generic/tclConfig.c
  generic/tclDate.c
  generic/tclDictObj.c
  generic/tclDisassemble.c
  generic/tclEncoding.c
  generic/tclEnsemble.c
  generic/tclEnv.c
  generic/tclEvent.c
  generic/tclExecute.c
  generic/tclFCmd.c
  generic/tclFileName.c
  generic/tclGet.c
  generic/tclHash.c
  generic/tclHistory.c
  generic/tclIndexObj.c
  generic/tclInterp.c
  generic/tclIO.c
  generic/tclIOCmd.c
  generic/tclIOGT.c
  generic/tclIOSock.c
  generic/tclIOUtil.c
  generic/tclIORChan.c
  generic/tclIORTrans.c
  generic/tclLink.c
  generic/tclListObj.c
  generic/tclLiteral.c
  generic/tclLoad.c
  generic/tclMain.c
  generic/tclNamesp.c
  generic/tclNotify.c
  generic/tclObj.c
  generic/tclOptimize.c
  generic/tclPanic.c
  generic/tclParse.c
  generic/tclPathObj.c
  generic/tclPipe.c
  generic/tclPkg.c
  generic/tclPkgConfig.c
  generic/tclPosixStr.c
  generic/tclPreserve.c
  generic/tclProc.c
  generic/tclRegexp.c
  generic/tclResolve.c
  generic/tclResult.c
  generic/tclScan.c
  generic/tclStubInit.c
  generic/tclStringObj.c
  generic/tclStrToD.c
  generic/tclTestObj.c
  generic/tclTestProcBodyObj.c
  generic/tclThread.c
  generic/tclThreadAlloc.c
  generic/tclThreadJoin.c
  generic/tclThreadStorage.c
  generic/tclTimer.c
  generic/tclTrace.c
  generic/tclUtf.c
  generic/tclUtil.c
  generic/tclVar.c
  generic/tclZlib.c
  generic/tclUniData.c
  )

SET(TCL_TEST_SRCS
  generic/tclTest.c
  generic/tclThreadTest.c
  unix/tclUnixTest.c
  unix/tclXtTest.c
  win/tclWinTest.c
  )

IF(TCL_TOMMATH)
  SET(TCL_GENERIC_SRCS ${TCL_GENERIC_SRCS} generic/tclTomMathInterface.c)
ENDIF(TCL_TOMMATH)

SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man") 
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
SET_PROPERTY(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man")


SET(TCL_OO_SRCS
  generic/tclOO.c
  generic/tclOOBasic.c
  generic/tclOOCall.c
  generic/tclOODefineCmds.c
  generic/tclOOInfo.c
  generic/tclOOMethod.c
  generic/tclOOStubInit.c
  )


SET(TCL_STUB_SRCS
  generic/tclStubLib.c
  generic/tclTomMathStubLib.c
  generic/tclOOStubLib.c
  )

SET(TCL_TOMMATH_SRCS
  libtommath/bncore.c
  libtommath/bn_reverse.c
  libtommath/bn_fast_s_mp_mul_digs.c
  libtommath/bn_fast_s_mp_sqr.c
  libtommath/bn_mp_add.c
  libtommath/bn_mp_add_d.c
  libtommath/bn_mp_and.c
  libtommath/bn_mp_clamp.c
  libtommath/bn_mp_clear.c
  libtommath/bn_mp_clear_multi.c
  libtommath/bn_mp_cmp.c
  libtommath/bn_mp_cmp_d.c
  libtommath/bn_mp_cmp_mag.c
  libtommath/bn_mp_copy.c
  libtommath/bn_mp_cnt_lsb.c
  libtommath/bn_mp_count_bits.c
  libtommath/bn_mp_div.c
  libtommath/bn_mp_div_d.c
  libtommath/bn_mp_div_2.c
  libtommath/bn_mp_div_2d.c
  libtommath/bn_mp_div_3.c
  libtommath/bn_mp_exch.c
  libtommath/bn_mp_expt_d.c
  libtommath/bn_mp_grow.c
  libtommath/bn_mp_init.c
  libtommath/bn_mp_init_copy.c
  libtommath/bn_mp_init_multi.c
  libtommath/bn_mp_init_set.c
  libtommath/bn_mp_init_set_int.c
  libtommath/bn_mp_init_size.c
  libtommath/bn_mp_karatsuba_mul.c
  libtommath/bn_mp_karatsuba_sqr.c
  libtommath/bn_mp_lshd.c
  libtommath/bn_mp_mod.c
  libtommath/bn_mp_mod_2d.c
  libtommath/bn_mp_mul.c
  libtommath/bn_mp_mul_2.c
  libtommath/bn_mp_mul_2d.c
  libtommath/bn_mp_mul_d.c
  libtommath/bn_mp_neg.c
  libtommath/bn_mp_or.c
  libtommath/bn_mp_radix_size.c
  libtommath/bn_mp_radix_smap.c
  libtommath/bn_mp_read_radix.c
  libtommath/bn_mp_rshd.c
  libtommath/bn_mp_set.c
  libtommath/bn_mp_set_int.c
  libtommath/bn_mp_shrink.c
  libtommath/bn_mp_sqr.c
  libtommath/bn_mp_sqrt.c
  libtommath/bn_mp_sub.c
  libtommath/bn_mp_sub_d.c
  libtommath/bn_mp_to_unsigned_bin.c
  libtommath/bn_mp_to_unsigned_bin_n.c
  libtommath/bn_mp_toom_mul.c
  libtommath/bn_mp_toom_sqr.c
  libtommath/bn_mp_toradix_n.c
  libtommath/bn_mp_unsigned_bin_size.c
  libtommath/bn_mp_xor.c
  libtommath/bn_mp_zero.c
  libtommath/bn_s_mp_add.c
  libtommath/bn_s_mp_mul_digs.c
  libtommath/bn_s_mp_sqr.c
  libtommath/bn_s_mp_sub.c
  )

SET(TCL_OSX_SRCS
  macosx/tclMacOSXBundle.c
  macosx/tclMacOSXFCmd.c
  macosx/tclMacOSXNotify.c
  )

SET(TCL_WIN_SRCS
  win/tcl.rc
  win/tclAppInit.c
  win/tclWin32Dll.c
  win/tclWinChan.c
  win/tclWinConsole.c
  win/tclWinDde.c
  win/tclWinError.c
  win/tclWinFCmd.c
  win/tclWinFile.c
  win/tclWinInit.c
  win/tclWinLoad.c
  win/tclWinNotify.c
  win/tclWinPipe.c
  win/tclWinReg.c
  win/tclWinSerial.c
  win/tclWinSock.c
  win/tclWinThrd.c
  win/tclWinTest.c
  win/tclWinTime.c
  )

IF(NOT LIB_DIR)
  IF(NOT WIN32)
    SET(LIB_DIR lib)
  ELSE(NOT WIN32)
    SET(LIB_DIR bin)
  ENDIF(NOT WIN32)
ENDIF(NOT LIB_DIR)


SET_PROPERTY(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
SET_PROPERTY(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")

SET(TCL_UNIX_SRCS
  unix/tclAppInit.c
  unix/tclUnixChan.c
  unix/tclUnixEvent.c
  unix/tclUnixFCmd.c
  unix/tclUnixFile.c
  unix/tclUnixPipe.c
  unix/tclUnixSock.c
  unix/tclUnixTest.c
  unix/tclUnixThrd.c
  unix/tclUnixTime.c
  unix/tclUnixInit.c
  unix/tclUnixCompat.c
  )

SET_PROPERTY(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
SET_PROPERTY(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")

SET(TCL_NOTIFY_SRCS
  unix/tclUnixNotfy.c
  )

SET(TCL_DL_SRCS
  unix/tclLoadAix.c
  unix/tclLoadDl.c
  unix/tclLoadDyld.c
  generic/tclLoadNone.c
  unix/tclLoadOSF.c
  unix/tclLoadShl.c
  )

IF(NOT ZLIB_FOUND)
  SET(TCL_ZLIB_SRCS
    compat/zlib/adler32.c 
    compat/zlib/compress.c 
    compat/zlib/crc32.c 
    compat/zlib/deflate.c 
    compat/zlib/infback.c 
    compat/zlib/inffast.c 
    compat/zlib/inflate.c 
    compat/zlib/inftrees.c 
    compat/zlib/trees.c 
    compat/zlib/uncompr.c 
    compat/zlib/zutil.c
    )
  SET(ZLIB_LIBRARY "")
  SET(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat/zlib)
ENDIF(NOT ZLIB_FOUND)

SET(TCL_SRCS ${TCL_GENERIC_SRCS} ${TCL_OO_SRCS} ${TCL_TOMMATH_SRCS} ${TCL_STUB_SRCS} ${TCL_ZLIB_SRCS}) 
IF(WIN32)
  SET(TCL_SRCS ${TCL_SRCS} ${TCL_WIN_SRCS})
ELSE(WIN32)
  SET(TCL_SRCS ${TCL_SRCS} ${TCL_NOTIFY_SRCS} ${TCL_UNIX_SRCS})
  IF(APPLE)
    SET(TCL_SRCS ${TCL_SRCS} ${TCL_OSX_SRCS} unix/tclLoadDyld.c)
  ENDIF(APPLE)
  IF(NOT APPLE)
    SET(TCL_SRCS ${TCL_SRCS} unix/tclLoadDl.c)
  ENDIF(NOT APPLE)
ENDIF(WIN32)

SET(TCL_INCLUDE_DIRS ${TCL_SOURCE_DIR}/generic ${TCL_SOURCE_DIR}/libtommath ${TCL_BINARY_DIR}/include)
IF(WIN32)
  SET(TCL_INCLUDE_DIRS ${TCL_SOURCE_DIR}/win ${TCL_INCLUDE_DIRS})
ELSE(WIN32)
  SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SOURCE_DIR}/unix)
ENDIF(WIN32)

include_directories(
  ${TCL_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  )

IF(WIN32)
  SET(TCLSH_SRCS win/tclAppInit.c win/tclsh.rc)
ELSE(WIN32)
  SET(TCLSH_SRCS unix/tclAppInit.c)
ENDIF(WIN32)

IF(MINGW)
  set(TCL_MINGW_LIBS ws2_32)
ENDIF(MINGW)

add_library(tcl ${TCL_SRCS})
target_link_libraries(tcl ${CoreFoundation_LIBRARIES} ${TCL_LINK_LIBS} ${DL_LIBRARY} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${TCL_MINGW_LIBS})
install(TARGETS tcl 
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})
GET_TARGET_PROPERTY(TCL_INSTALL_LIBNAME tcl LOCATION)
SET_TARGET_PROPERTIES(tcl PROPERTIES VERSION ${TCL_VERSION} SOVERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})
IF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
  SET_TARGET_PROPERTIES(tcl PROPERTIES LINK_FLAGS "-compatibility_version ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR} -current_version ${TCL_VERSION} -install_name \"${TCL_INSTALL_LIBNAME}\" -seg1addr 0xa000000 -sectcreate __TEXT __info_plist \"${TCL_BINARY_DIR}/Tcl-Info.plist\"")
  SET(EXTRA_TCLSH_LIBS "-sectcreate __TEXT __info_plist \"${TCL_BINARY_DIR}/Tclsh-Info.plist\"")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tcl-Info.plist.in ${TCL_BINARY_DIR}/Tcl-Info.plist)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tclsh-Info.plist.in ${TCL_BINARY_DIR}/Tclsh-Info.plist)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")


add_library(tclstub STATIC ${TCL_STUB_SRCS})
target_link_libraries(tclstub ${CMAKE_THREAD_LIBS_INIT} ${CoreFoundation_LIBRARIES} ${DL_LIBRARY} ${ZLIB_LIBRARY})
CHECK_C_COMPILER_FLAG(-fPIC FPIC_COMPILER_FLAG)
if(FPIC_COMPILER_FLAG)
  set_target_properties(tclstub PROPERTIES COMPILE_FLAGS "-fPIC")
endif(FPIC_COMPILER_FLAG)
install(TARGETS tclstub
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl ${M_LIBRARY} ${ZLIB_LIBRARY} ${TCL_THREADS_LIB} ${EXTRA_TCLSH_LIBS})
install(TARGETS tclsh DESTINATION bin)
SET_TARGET_PROPERTIES(tclsh PROPERTIES VERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})

SET(TCL_HDRS
  generic/tcl.h
  generic/tclDecls.h
  generic/tclPlatDecls.h
  generic/tclTomMath.h
  generic/tclTomMathDecls.h
  generic/tclOO.h
  generic/tclOODecls.h
  )
install(FILES ${TCL_HDRS} DESTINATION include)

MARK_AS_ADVANCED(TCL_LIBRARIES TCL_CONF_PREFIX TCL_INCLUDE_DIRS TCL_INCLUDE_PATH)
MARK_AS_ADVANCED(TCL_STUB_LIBRARIES TCL_TCLSH TCL_TCLSH_EXECUTABLE)
MARK_AS_ADVANCED(TCL_TK_CONF_PREFIX TCL_TOMMATH)
MARK_AS_ADVANCED(TCL_VERSION_MAJOR TCL_VERSION_MINOR TCL_WISH_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

