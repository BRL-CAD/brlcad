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
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(TCL)

# build shared libs by default
if (NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build shared libraries" ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

# build static libs by default
if (NOT DEFINED BUILD_STATIC_LIBS)
  option(BUILD_STATIC_LIBS "Build shared libraries" ON)
endif (NOT DEFINED BUILD_STATIC_LIBS)

# version numbers
set(TCL_VERSION_MAJOR 8)
set(TCL_VERSION_MINOR 6)
set(TCL_VERSION_PATCH 10)
set(TCL_VERSION "${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}.${TCL_VERSION_PATCH}")

# For Windows, we need the Resource Compiler language
if (WIN32)
  enable_language(RC)
endif (WIN32)

#-----------------------------------------------------------------------------
# Set CMake module path
list(APPEND CMAKE_MODULE_PATH "${TCL_SOURCE_DIR}/CMake")

#---------------------------------------------------------------------
# Define relative install locations and output directories.  Don't set
# these if they have already been set by some other means (like a
# higher level CMakeLists.txt file including this one).

include(Path_Setup)

#-----------------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in the build
# directory AND install directory. Documentation of these options is available
# at http://www.cmake.org/Wiki/CMake_RPATH_handling
include(RPath_Setup)
cmake_set_rpath()

#----------------------------------------------------------------------------
# Define some standard configuration settings passed to all Tcl build targets

# We're building Tcl
add_definitions(-DBUILD_tcl)

# First, get some standard settings out of the way
# Assume we have STDC_HEADERS until we meet a situation where we don't
add_definitions(-DSTDC_HEADERS=1)
# Get the SHLIB extension from CMake
add_definitions(-DTCL_SHLIB_EXT="${CMAKE_SHARED_LIBRARY_SUFFIX}")

# Tom's Math Library is always enabled...
add_definitions(-DTCL_TOMMATH=1)
add_definitions(-DMP_PREC=4)
add_definitions(-DMP_FIXED_CUTOFFS)
add_definitions(-DMP_NO_STDINT)

#----------------------------------------------------------------------------
# Define some platform specific flags - ideally we'd configure test for all
# of these, but until then just do what's needed.

# Windows specific flags
if (MSVC)
  add_definitions(-D_UNICODE -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP)
  add_definitions(-DTCL_PIPE_DLL="tclpip${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}.dll")
  add_definitions(-Dinline=__inline)
endif (MSVC)
function(TCL_WIN_FLAGS srcfiles)
  if (MSVC)
    foreach(srcfile ${${srcfiles}})
      set_property(SOURCE ${srcfile} APPEND_STRING PROPERTY COMPILE_DEFINITIONS "-Ot -Oi -fp:strict -Gs -GS -GL -MD")
    endforeach(srcfile ${${srcfiles}})
  endif (MSVC)
endfunction(TCL_WIN_FLAGS)

if (MINGW)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEP)
  add_definitions(-DTCL_PIPE_DLL="tclpip${TCL_VERSION_MAJOR}${TCL_VERSION_MINOR}.dll")
  add_definitions(-Dinline=__inline)
  remove_definitions(-w)
  add_definitions(-DHAVE_NO_SEH -DEXCEPTION_DISPOSITION=int)
endif (MINGW)

if (APPLE)
  add_definitions(-DMAC_OSX_TCL=1)
endif (APPLE)

# Embedded configuration information, encoding to use for the values, TIP #59
if (WIN32)
  add_definitions(-DTCL_CFGVAL_ENCODING="cp1252")
else(WIN32)
  add_definitions(-DTCL_CFGVAL_ENCODING="iso8859-1")
endif (WIN32)

#------------------------------------------------------------------------
# Options
#------------------------------------------------------------------------
option(TCL_COMPILATION_TRACING "Bytecode compilation debugging" OFF)
if (TCL_COMPILATION_TRACING)
  add_definitions(-DTCL_COMPILE_DEBUG=1)
else(TCL_COMPILATION_TRACING)
  add_definitions(-DTCL_COMPILE_DEBUG=0)
endif (TCL_COMPILATION_TRACING)

option(TCL_COMPILE_STATS "Bytecode statistics" OFF)
if (TCL_COMPILE_STATS)
  add_definitions(-DTCL_COMPILE_STATS=1)
endif (TCL_COMPILE_STATS)

#------------------------------------------------------------------------
# Test compiler flags
#------------------------------------------------------------------------

# Check if the compiler understands -pipe.  If so, use it.
# It makes compiling go faster.  (This is only a performance feature.)
include(CheckCCompilerFlag)
check_c_compiler_flag(-pipe PIPE_COMPILER_FLAG)
if (PIPE_COMPILER_FLAG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")
endif (PIPE_COMPILER_FLAG)

# Position independent code
check_c_compiler_flag(-fPIC FPIC_COMPILER_FLAG)

#------------------------------------------------------------------------
# Threads support
#------------------------------------------------------------------------
# Unless something really unusual is going on, we're going to want
# support for threads.
include(CheckFunctionExists)
function(check_extra_pthread_funcs)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
  check_function_exists(pthread_mutex_init HAVE_PTHREAD_MUTEX_INIT)
  if (HAVE_PTHREAD_MUTEX_INIT)
    add_definitions(-DHAVE_PTHREAD_MUTEX_INIT=1)
  endif (HAVE_PTHREAD_MUTEX_INIT)
  check_function_exists(pthread_attr_setstacksize HAVE_PTHREAD_ATTR_SETSTACKSIZE)
  if (HAVE_PTHREAD_ATTR_SETSTACKSIZE)
    add_definitions(-DHAVE_PTHREAD_ATTR_SETSTACKSIZE=1)
  endif (HAVE_PTHREAD_ATTR_SETSTACKSIZE)
  check_function_exists(pthread_atfork HAVE_PTHREAD_ATFORK)
  if (HAVE_PTHREAD_ATFORK)
    add_definitions(-DHAVE_PTHREAD_ATFORK=1)
  endif (HAVE_PTHREAD_ATFORK)
endfunction(check_extra_pthread_funcs)

find_package(Threads REQUIRED)
add_definitions(-DTCL_THREADS=1)
add_definitions(-DUSE_THREAD_ALLOC=1)
add_definitions(-D_REENTRANT=1)
add_definitions(-D_THREAD_SAFE=1)
check_extra_pthread_funcs()

#--------------------------------------------------------------------
# Header checks.
#--------------------------------------------------------------------
include(CMakeParseArguments)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)

function(Tcl_Check_Include_File filename)
  cmake_parse_arguments(OPT "NEG_FLAG;USE" "TEST_SRC;COMPAT_SRC;DEFVAR" "" ${ARGN})
  string(REPLACE "." "_" var "${filename}")
  string(REPLACE "/" "_" var "${var}")
  string(TOUPPER "${var}" var)
  check_include_files(${filename} HAVE_${var})
  if (HAVE_${var})
    if (OPT_DEFVAR)
      set(CVAR ${OPT_DEFVAR})
    else(OPT_DEFVAR)
      set(CVAR HAVE_${var})
    endif (OPT_DEFVAR)
    if (OPT_USE)
      # Usability test requested - we're not done yet.
      if (OPT_TEST_SRC)
	check_c_source_compiles("${${OPT_TEST_SRC}}" ${var}_USABLE)
      else(OPT_TEST_SRC)
	check_c_source_compiles("#include <${filename}>\nint main() {return 0;}" ${var}_USABLE)
      endif (OPT_TEST_SRC)
      if (${var}_USABLE)
	set(HAVE_${var} 1 PARENT_SCOPE)
	set(${CVAR} 1 PARENT_SCOPE)
	add_definitions(-D${CVAR}=1)
      else(${var}_USABLE)
	if (OPT_NEG_FLAG)
	  add_definitions(-DNO_${var}=1)
	endif (OPT_NEG_FLAG)
	if (OPT_COMPAT_SRC)
	  set(USE_COMPAT 1 PARENT_SCOPE)
	  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC})
	    message(FATAL_ERROR "Specified compatibilty file ${OPT_COMPAT_SRC}, but ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC} was not found.")
	  endif (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC})
	endif (OPT_COMPAT_SRC)
      endif (${var}_USABLE)
    else(OPT_USE)
      set(HAVE_${var} 1 PARENT_SCOPE)
      set(${CVAR} 1 PARENT_SCOPE)
      add_definitions(-D${CVAR}=1)
    endif (OPT_USE)
  else(HAVE_${var})
    if (OPT_NEG_FLAG)
      add_definitions(-DNO_${var}=1)
    endif (OPT_NEG_FLAG)
    if (OPT_COMPAT_SRC)
      set(USE_COMPAT 1 PARENT_SCOPE)
      if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC})
	message(FATAL_ERROR "Specified compatibilty file ${OPT_COMPAT_SRC}, but ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC} was not found.")
      endif (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${OPT_COMPAT_SRC})
    endif (OPT_COMPAT_SRC)
  endif (HAVE_${var})
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

Tcl_Check_Include_File(AvailabilityMacros.h DEFVAR HAVE_AVAILABILITY_MACROS_H)
Tcl_Check_Include_File(copyfile.h)
Tcl_Check_Include_File(dirent.h NEG_FLAG USE TEST_SRC DIRENT_TEST_SRCS COMPAT_SRC compat/dirent.h)
Tcl_Check_Include_File(dlfcn.h NEG_FLAG USE COMPAT_SRC compat/dlfcn.h)
Tcl_Check_Include_File(float.h NEG_FLAG USE COMPAT_SRC compat/float.h)
Tcl_Check_Include_File(inttypes.h)
Tcl_Check_Include_File(langinfo.h USE TEST_SRC LANG_TEST_SRCS DEFVAR HAVE_LANGINFO)
Tcl_Check_Include_File(limits.h USE)
Tcl_Check_Include_File(memory.h)
Tcl_Check_Include_File(net/errno.h)
Tcl_Check_Include_File(stdint.h)
Tcl_Check_Include_File(stdlib.h)
Tcl_Check_Include_File(string.h)
Tcl_Check_Include_File(strings.h)
Tcl_Check_Include_File(sys/filio.h)
Tcl_Check_Include_File(sys/ioctl.h)
Tcl_Check_Include_File(sys/modem.h)
Tcl_Check_Include_File(sys/param.h USE)
Tcl_Check_Include_File(sys/sdt.h)
Tcl_Check_Include_File(sys/stat.h)
Tcl_Check_Include_File(sys/time.h)
Tcl_Check_Include_File(sys/types.h)
Tcl_Check_Include_File(sys/wait.h USE)
Tcl_Check_Include_File(termios.h)
Tcl_Check_Include_File(unistd.h COMPAT_SRC compat/unistd.h)
Tcl_Check_Include_File(values.h USE)

#--------------------------------------------------------------------
# Function checks.
#--------------------------------------------------------------------
include(CMakeParseArguments)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

function(Tcl_Check_Function_Exists fname)
  cmake_parse_arguments(OPT "NEG_FLAG" "COMPAT_SRC;DEFVAR" "" ${ARGN})
  string(TOUPPER "${fname}" var)
  check_function_exists(${fname} HAVE_${var})
  if (HAVE_${var})
    set(HAVE_${var} 1 PARENT_SCOPE)
    if (OPT_DEFVAR)
      set(CVAR ${OPT_DEFVAR})
    else(OPT_DEFVAR)
      set(CVAR HAVE_${var})
    endif (OPT_DEFVAR)
    # If we have a compat src file, there's no need for a flag -
    # Tcl is ensuring this exists, so there's not source code level
    # ifdef to satisfy.
    if (NOT OPT_COMPAT_SRC)
      add_definitions(-D${CVAR}=1)
    endif (NOT OPT_COMPAT_SRC)
  else(HAVE_${var})
    if (OPT_NEG_FLAG)
      add_definitions(-DNO_${var}=1)
    endif (OPT_NEG_FLAG)
    if (OPT_COMPAT_SRC)
      set(COMPAT_SRCS "${COMPAT_SRCS}" ${OPT_COMPAT_SRC} PARENT_SCOPE)
    endif (OPT_COMPAT_SRC)
  endif (HAVE_${var})
endfunction(Tcl_Check_Function_Exists)

Tcl_Check_Function_Exists(getcwd)
if (NOT HAVE_GETCWD)
  Tcl_Check_Function_Exists(getwd HAVE_GETWD)
  if (NOT HAVE_GETWD)
    add_definitions(-DNO_GETWD=1)
  else(NOT HAVE_GETWD)
    add_definitions(-DUSEGETWD=1)
  endif (NOT HAVE_GETWD)
endif (NOT HAVE_GETCWD)

Tcl_Check_Function_Exists(hypot COMPAT_SRC compat/hypot.c)
Tcl_Check_Function_Exists(mkstemp COMPAT_SRC compat/mkstemp.c)
Tcl_Check_Function_Exists(opendir COMPAT_SRC compat/opendir.c)
Tcl_Check_Function_Exists(waitpid COMPAT_SRC compat/waitpid.c)
Tcl_Check_Function_Exists(memcmp COMPAT_SRC compat/memcmp.c)
Tcl_Check_Function_Exists(wait3 NEG_FLAG)
Tcl_Check_Function_Exists(uname NEG_FLAG)
Tcl_Check_Function_Exists(realpath NEG_FLAG)
Tcl_Check_Function_Exists(fstatfs NEG_FLAG)
Tcl_Check_Function_Exists(chflags)
Tcl_Check_Function_Exists(mkstemps)

# Check for early Darwin version here - realpath
# is not threadsafe prior to Darwin 7
if (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND HAVE_REALPATH)
  string(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
  if (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
    message("realpath is not threadsafe in Darwin versions prior to 7, disabling")
    set(HAVE_REALPATH)
    remove_definitions(-DHAVE_REALPATH=1)
    add_definitions(-DNO_REALPATH=1)
  endif (${CMAKE_SYSTEM_MAJOR_VERSION} LESS 7)
endif (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$" AND HAVE_REALPATH)

Tcl_Check_Function_Exists(getaddrinfo HAVE_GETADDRINFO)
if (HAVE_GETADDRINFO)
  set(GETADDRINFO_SRC "
#include <netdb.h>
int main () {
  const char *name, *port;
  struct addrinfo *aiPtr, hints;
  (void)getaddrinfo(name,port, &hints, &aiPtr);
  (void)freeaddrinfo(aiPtr);
  return 0;
}
")
  CHECK_C_SOURCE_COMPILES("${GETADDRINFO_SRC}" WORKING_GETADDRINFO)
  if (WORKING_GETADDRINFO)
    add_definitions(-DHAVE_GETADDRINFO=1)
  endif (WORKING_GETADDRINFO)
endif (HAVE_GETADDRINFO)

#--------------------------------------------------------------------
# Library checks.
#--------------------------------------------------------------------
include(CMakeParseArguments)
include(CheckLibraryExists)
include(CheckCSourceCompiles)

check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(M_LIBRARY "m")
endif (HAVE_M_LIBRARY)

# Note: Do we ever NOT need CoreFoundation on OSX?
if (APPLE)
  include(CMakeFindFrameworks)
  cmake_find_frameworks(CoreFoundation)
  if (CoreFoundation_FRAMEWORKS)
    set(CoreFoundation_LIBRARIES "-framework CoreFoundation")
    add_definitions(-DHAVE_COREFOUNDATION=1)
  endif (CoreFoundation_FRAMEWORKS)
endif (APPLE)

# We're going to have zlib one way or the other - always define
# HAVE_ZLIB to be on.
find_package(ZLIB)
add_definitions(-DHAVE_ZLIB=1)

# Need dynamic loading and unloading... always on until we find
# a case where we need to disable it.
check_library_exists(dl dlopen "" HAVE_DL_LIBRARY)
if (HAVE_DL_LIBRARY)
  set(DL_LIBRARY "dl")
endif (HAVE_DL_LIBRARY)

# Support unloading dlls
add_definitions(-DTCL_UNLOAD_DLLS=1)

#-----------------------------------------------------------
# There are a number of possibilities for sockets
Tcl_Check_Function_Exists(connect)
if (HAVE_CONNECT)
  set(NEED_SOCKET_LIBRARY 0)
else(HAVE_CONNECT)
  set(NEED_SOCKET_LIBRARY 1)
endif (HAVE_CONNECT)

# Checking for socket
if (NEED_SOCKET_LIBRARY)
  check_library_exists(socket conect "" HAVE_SOCKET_LIBRARY)
  if (HAVE_SOCKET_LIBRARY)
    set(SOCKET_LIBRARY "socket")
    set(NEED_SOCKET_LIBRARY 0)
  endif (HAVE_SOCKET_LIBRARY)
endif (NEED_SOCKET_LIBRARY)

# Checking for inet
if (NEED_SOCKET_LIBRARY)
  check_library_exists(inet main "" HAVE_INET_LIBRARY)
  if (HAVE_INET_LIBRARY)
    set(SOCKET_LIBRARY "inet")
    set(NEED_SOCKET_LIBRARY 0)
  endif (HAVE_INET_LIBRARY)
endif (NEED_SOCKET_LIBRARY)

#-----------------------------------------------------------
# gethostbyname
Tcl_Check_Function_Exists(gethostbyname)
if (HAVE_GETHOSTBYNAME)
  set(NEED_GHBN_LIBRARY 0)
else(HAVE_GETHOSTBYNAME)
  set(NEED_GHBN_LIBRARY 1)
endif (HAVE_GETHOSTBYNAME)

if (NEED_GHBN_LIBRARY)
  check_library_exists(nsl gethostbyname "" HAVE_NSL_LIBRARY)
  if (HAVE_INET_LIBRARY)
    set(GHBN_LIBRARY "inet")
    set(NEED_GHBN_LIBRARY 0)
  endif (HAVE_INET_LIBRARY)
endif (NEED_GHBN_LIBRARY)

#--------------------------------------------------------------------
# 64 bit
#--------------------------------------------------------------------
function(tcl_check_c_flag flag VAR)
  if (NOT DEFINED ${VAR})
    string(TOUPPER ${flag} UPPER_FLAG)
    string(REGEX REPLACE " " "_" UPPER_FLAG ${UPPER_FLAG})
    string(REGEX REPLACE "=" "_" UPPER_FLAG ${UPPER_FLAG})
    check_c_compiler_flag(-${flag} ${UPPER_FLAG}_COMPILER_FLAG)
    if (${UPPER_FLAG}_COMPILER_FLAG)
      set(${VAR} "-${flag}" PARENT_SCOPE)
    endif (${UPPER_FLAG}_COMPILER_FLAG)
  endif (NOT DEFINED ${VAR})
endfunction(tcl_check_c_flag flag VAR)

if (${CMAKE_SIZEOF_VOID_P} MATCHES "^8$" OR CMAKE_CL_64)
  if (NOT DEFINED 64BIT_FLAG)
    tcl_check_c_flag("arch x86_64" 64BIT_FLAG)
    tcl_check_c_flag("64" 64BIT_FLAG)
    tcl_check_c_flag("mabi=64" 64BIT_FLAG)
    tcl_check_c_flag("m64" 64BIT_FLAG)
    tcl_check_c_flag("q64" 64BIT_FLAG)
  endif (NOT DEFINED 64BIT_FLAG)
  if (CMAKE_CL_64)
    add_definitions(-D_stati64=_stat64)
  endif (CMAKE_CL_64)
endif (${CMAKE_SIZEOF_VOID_P} MATCHES "^8$" OR CMAKE_CL_64)

#--------------------------------------------------------------------
# optimization and debugging
#--------------------------------------------------------------------
check_c_compiler_flag(-g HAVE_G_FLAG)
if (HAVE_G_FLAG)
  add_definitions(-DTCL_CFG_DEBUG=1)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif (HAVE_G_FLAG)

check_c_compiler_flag(-O2 HAVE_O2_FLAG)
if (HAVE_O2_FLAG)
  add_definitions(-DTCL_CFG_OPTIMIZED=1)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
endif (HAVE_O2_FLAG)

#-------------------------------------------------------------------- 
#       Check endianness because we can optimize comparisons of
#       Tcl_UniChar strings to memcmp on big-endian systems.
#--------------------------------------------------------------------
if (NOT WIN32)
  include(TestBigEndian)
  test_big_endian(WORDS_BIGENDIAN)
  if (WORDS_BIGENDIAN)
    add_definitions(-DWORDS_BIGENDIAN=1)
  endif (WORDS_BIGENDIAN)
endif (NOT WIN32)

#--------------------------------------------------------------------
# If we ended up enabling threads, look for thread safe variations
# of some functions
function(tcl_5_4_arg_functest func srcs5 srcs4)
  string(TOUPPER "${func}" UPPERFUNC)
  Tcl_Check_Function_Exists(${func})
  if (HAVE_${UPPERFUNC})
    set(HAVE_${UPPERFUNC} 1 PARENT_SCOPE)
    # Do we have the 5 arg or the 4 arg version?
    check_c_source_compiles("${${srcs5}}" HAVE_${UPPERFUNC}_5)
    if (HAVE_${UPPERFUNC}_5)
      set(HAVE_${UPPERFUNC}_5 1 PARENT_SCOPE)
      add_definitions(-DHAVE_${UPPERFUNC}_5=1)
    else(HAVE_${UPPERFUNC}_5)
      check_c_source_compiles("${${srcs4}}" HAVE_${UPPERFUNC}_4)
      if (HAVE_${UPPERFUNC}_4)
	set(HAVE_${UPPERFUNC}_4 1 PARENT_SCOPE)
	add_definitions(-DHAVE_${UPPERFUNC}_4=1)
      endif (HAVE_${UPPERFUNC}_4)
    endif (HAVE_${UPPERFUNC}_5)
  endif (HAVE_${UPPERFUNC})
endfunction(tcl_5_4_arg_functest)

if (NOT WIN32)

  #------------------------------------------------------------------
  # getpwuid_r
  set(getpwuid_r_5_src "
#include <sys/types.h>
#include <pwd.h>
int main(){
  uid_t uid; struct passwd pw, *pwp;
  char buf[512]; int buflen = 512;
  (void) getpwuid_r(uid, &pw, buf, buflen, &pwp);
return 0;}
  ")
  set(getpwuid_r_4_src "
#include <sys/types.h>
#include <pwd.h>
int main(){
  uid_t uid; struct passwd pw;
  char buf[512]; int buflen = 512;
  (void) getpwuid_r(uid, &pw, buf, buflen);
return 0;}
  ")
  tcl_5_4_arg_functest(getpwuid_r getpwuid_r_5_src getpwuid_r_4_src)

  #------------------------------------------------------------------
  # getpwnam_r
  set(getpwnam_r_5_src "
#include <sys/types.h>
#include <pwd.h>
int main(){
  char *name; struct passwd pw, *pwp;
  char buf[512]; int buflen = 512;
  (void) getpwnam_r(name, &pw, buf, buflen, &pwp);
return 0;}
  ")
  set(getpwnam_r_4_src "
#include <sys/types.h>
#include <pwd.h>
int main(){
  char *name; struct passwd pw;
  char buf[512]; int buflen = 512;
  (void)getpwnam_r(name, &pw, buf, buflen);
return 0;}
  ")
  tcl_5_4_arg_functest(getpwnam_r getpwnam_r_5_src getpwnam_r_4_src)

  #------------------------------------------------------------------
  # getgrgid_r
  set(getgrgid_r_5_src "
#include <sys/types.h>
#include <grp.h>
int main(){
  gid_t gid; struct group gr, *grp;
  char buf[512]; int buflen = 512;
  (void) getgrgid_r(gid, &gr, buf, buflen, &grp);
return 0;}
  ")
  set(getgrgid_r_4_src "
#include <sys/types.h>
#include <grp.h>
int main(){
  gid_t gid; struct group gr;
  char buf[512]; int buflen = 512;
  (void)getgrgid_r(gid, &gr, buf, buflen);
return 0;}
  ")
  tcl_5_4_arg_functest(getgrgid_r getgrgid_r_5_src getgrgid_r_4_src)

  #------------------------------------------------------------------
  # getgrnam_r
  set(getgrnam_r_5_src "
#include <sys/types.h>
#include <grp.h>
int main(){
  char *name; struct group gr, *grp;
  char buf[512]; int buflen = 512;
  (void) getgrnam_r(name, &gr, buf, buflen, &grp);
return 0;}
  ")
  set(getgrnam_r_4_src "
#include <sys/types.h>
#include <grp.h>
int main(){
  char *name; struct group gr;
  char buf[512]; int buflen = 512;
  (void)getgrnam_r(name, &gr, buf, buflen);
return 0;}
  ")
  tcl_5_4_arg_functest(getgrnam_r getgrnam_r_5_src getgrnam_r_4_src)

  #------------------------------------------------------------------
  # gethostbyaddr_r

  set(gethostbyaddr_r_7_src "
#include <netdb.h>
int main(){
  char *addr; int length; int type; struct hostent *result;
  char buffer[2048]; int buflen = 2048; int h_errnop;
  (void) gethostbyaddr_r(addr, length, type, result, buffer, buflen, &h_errnop);
return 0;}
  ")
  set(gethostbyaddr_r_8_src "
#include <netdb.h>
int main(){
  char *addr; int length; int type; struct hostent *result, *resultp;
  char buffer[2048]; int buflen = 2048; int h_errnop;
  (void) gethostbyaddr_r(addr, length, type, result, buffer, buflen, &resultp, &h_errnop);
return 0;}
  ")
  Tcl_Check_Function_Exists(gethostbyaddr_r)
  if (HAVE_GETHOSTBYADDR_R)
    check_c_source_compiles("${gethostbyaddr_r_7_src}" HAVE_GETHOSTBYADDR_R_7)
    if (HAVE_GETHOSTBYADDR_R_7)
      add_definitions(-DHAVE_GETHOSTBYADDR_R_7=1)
    else(HAVE_GETHOSTBYADDR_R_7)
      check_c_source_compiles("${gethostbyaddr_r_8_src}" HAVE_GETHOSTBYADDR_R_8)
      if (HAVE_GETHOSTBYADDR_R_8)
	add_definitions(-DHAVE_GETHOSTBYADDR_R_8=1)
      endif (HAVE_GETHOSTBYADDR_R_8)
    endif (HAVE_GETHOSTBYADDR_R_7)
  endif (HAVE_GETHOSTBYADDR_R)

  #------------------------------------------------------------------
  # gethostbyname_r
  set(gethostbyname_r_6_src "
#include <netdb.h>
int main(){
  char *name; struct hostent *he, *res; char buffer[2048];
  int buflen = 2048; int h_errnop;
  (void) gethostbyname_r(name, he, buffer, buflen, &res, &h_errnop);
return 0;}
  ")
  set(gethostbyname_r_5_src "
#include <netdb.h>
int main(){
  char *name; struct hostent *he; char buffer[2048];
  int buflen = 2048; int h_errnop;
  (void) gethostbyname_r(name, he, buffer, buflen, &h_errnop);
return 0;}
  ")
  set(gethostbyname_r_3_src "
#include <netdb.h>
int main(){
  char *name; struct hostent *he; struct hostent_data data;
  (void) gethostbyname_r(name, he, &data);
return 0;}
  ")
  Tcl_Check_Function_Exists(gethostbyname_r)
  if (HAVE_GETHOSTBYNAME_R)
    check_c_source_compiles("${gethostbyname_r_6_src}" HAVE_GETHOSTBYNAME_R_6)
    if (HAVE_GETHOSTBYNAME_R_6)
      add_definitions(-DHAVE_GETHOSTBYNAME_R_6=1)
    else(HAVE_GETHOSTBYNAME_R_6)
      check_c_source_compiles("${gethostbyname_r_5_src}" HAVE_GETHOSTBYNAME_R_5)
      if (HAVE_GETHOSTBYNAME_R_5)
	add_definitions(-DHAVE_GETHOSTBYNAME_R_5=1)
      else(HAVE_GETHOSTBYNAME_R_5)
	check_c_source_compiles("${gethostbyname_r_3_src}" HAVE_GETHOSTBYNAME_R_3)
	if (HAVE_GETHOSTBYNAME_R_3)
	  add_definitions(-DHAVE_GETHOSTBYNAME_R_3=1)
	endif (HAVE_GETHOSTBYNAME_R_3)
      endif (HAVE_GETHOSTBYNAME_R_5)
    endif (HAVE_GETHOSTBYNAME_R_6)
  endif (HAVE_GETHOSTBYNAME_R)


  # Darwin srcs take extra flags (TODO - is this still needed in 8.6?)
  if (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
    string(REGEX REPLACE "\\..*" "" CMAKE_SYSTEM_MAJOR_VERSION ${CMAKE_SYSTEM_VERSION})
    if (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYNAME=1)
      add_definitions(-DHAVE_MTSAFE_GETHOSTBYADDR=1)
    endif (${CMAKE_SYSTEM_MAJOR_VERSION} GREATER 5)
  endif (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

endif (NOT WIN32)

#--------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/tcl.cmake")

#------------------------------------------------------------------------------
#       Find out all about time handling differences.
#------------------------------------------------------------------------------
SC_TIME_HANDLER()

#---------------------------------------------------------------------------
# Check for stat structure files and blkcnt_t
#---------------------------------------------------------------------------

TCL_CHECK_STRUCT_HAS_MEMBER("struct stat" st_blocks sys/stat.h HAVE_STRUCT_STAT_ST_BLOCKS)
TCL_CHECK_STRUCT_HAS_MEMBER("struct stat" st_blksize sys/stat.h HAVE_STRUCT_STAT_ST_BLKSIZE)
CHECK_TYPE_SIZE(blkcnt_t HAVE_BLKCNT_T)
if (HAVE_BLKCNT_T)
  add_definitions(-DHAVE_BLKCNT_T=1)
endif (HAVE_BLKCNT_T)

#---------------------------------------------------------------------------
# Check for memmove
#---------------------------------------------------------------------------
# need to revisit this, not finding memmove
#TCL_CHECK_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)
#if (NOT HAVE_MEMMOVE)
#	add_definitions(-DNO_MEMMOVE=1)
#	TCL_CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
#	if (NOT HAVE_STRING_H)
#		add_definitions(-DNO_STRING_H=1)
#	endif (NOT HAVE_STRING_H)
#endif (NOT HAVE_MEMMOVE)

#--------------------------------------------------------------------
#       Check for various typedefs and provide substitutes if
#       they don't exist.
#--------------------------------------------------------------------
TCL_CHECK_TYPE_SIZE(mode_t HAVE_MODE)
TCL_CHECK_TYPE_SIZE(pid_t HAVE_PID)
TCL_CHECK_TYPE_SIZE(size_t HAVE_SIZE)
TCL_CHECK_TYPE_SIZE(uid_t HAVE_UID)
# The following tests may need to be more elaborate to function properly
TCL_CHECK_TYPE_SIZE(socklen_t HAVE_SOCKLEN sys/types.h sys/socket.h)
TCL_CHECK_TYPE_SIZE(intptr_t HAVE_INTPTR)
TCL_CHECK_TYPE_SIZE(uintptr_t HAVE_UINTPTR)

if (NOT HAVE_OPENDIR)
  add_definitions(-DUSE_DIRENT2_H=1)
endif (NOT HAVE_OPENDIR)

#----------------------------------------------------------------------------
# Check for strncasecmp
#----------------------------------------------------------------------------
CHECK_FUNCTION_EXISTS(strncasecmp HAVE_STRNCASECMP)
if (NOT HAVE_STRNCASECMP)
  CHECK_LIBRARY_EXISTS(socket strncasecmp "" HAVE_SOCKET_STRNCASECMP)
  CHECK_LIBRARY_EXISTS(inet strncasecmp "" HAVE_INET_STRNCASECMP)
  if (NOT HAVE_SOCKET_STRNCASECMP AND NOT HAVE_INET_STRNCASECMP)
    set(COMPAT_SRCS ${COMPAT_SRCS} compat/strncasecmp.c)
  endif (NOT HAVE_SOCKET_STRNCASECMP AND NOT HAVE_INET_STRNCASECMP)
endif (NOT HAVE_STRNCASECMP)

#----------------------------------------------------------------------------
# gettimeofday issues
#----------------------------------------------------------------------------
TCL_CHECK_FUNCTION_EXISTS(BSDgettimeofday HAVE_BSDGETTIMEOFDAY)
if (NOT HAVE_BSDGETTIMEOFDAY)
  CHECK_FUNCTION_EXISTS(gettimeofday HAVE_GETTIMEOFDAY)
  if (NOT HAVE_GETTIMEOFDAY)
    add_definitions(-DNO_GETTOD=1)
  endif (NOT HAVE_GETTIMEOFDAY)
endif (NOT HAVE_BSDGETTIMEOFDAY)
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(gettimeofday "sys/time.h" IS_GETTOD_DECLARED)
if (NOT IS_GETTOD_DECLARED)
  add_definitions(-DGETTOD_NOT_DECLARED=1)
endif (NOT IS_GETTOD_DECLARED)


#----------------------------------------------------------------------------
# signed/unsigned char - does the Tcl code still need this? See
# http://lists.gnu.org/archive/html/autoconf/2008-06/msg00054.html
#----------------------------------------------------------------------------
set(unsigned_char_srcs "
int main () {
   static int test_array [1 - 2 * !(((char) -1) < 0)];
   test_array [0] = 0;
   return 0;
}
")
if (NOT DEFINED CHAR_IS_UNSIGNED)
   CHECK_C_SOURCE_RUNS("${unsigned_char_srcs}" CHAR_IS_UNSIGNED)
endif (NOT DEFINED CHAR_IS_UNSIGNED)
if (CHAR_IS_UNSIGNED)
  add_definitions(-D__CHAR_UNSIGNED__)
endif (CHAR_IS_UNSIGNED)
set(signed_char_srcs "
int main () {
   signed char *p;
	p = 0;
	return 0;
}
")
CHECK_C_SOURCE_COMPILES("${signed_char_srcs}" HAVE_SIGNED_CHAR)
if (HAVE_SIGNED_CHAR)
  add_definitions(-DHAVE_SIGNED_CHAR=1)
endif (HAVE_SIGNED_CHAR)

#--------------------------------------------------------------------
# Check for chflags and mkstemps
#--------------------------------------------------------------------

#--------------------------------------------------------------------
# Check for support of isnan() function or macro
#--------------------------------------------------------------------
set(isnan_srcs "
#include <math.h>
int main () {
   isnan(0.0); /* Generates an error if isnan is missing */
	return 0;
}
")
if (NOT DEFINED USABLE_ISNAN)
  CHECK_C_SOURCE_RUNS("${isnan_srcs}" USABLE_ISNAN)
endif (NOT DEFINED USABLE_ISNAN)
if (NOT USABLE_ISNAN)
  add_definitions(-DNO_ISNAN=1)
endif (NOT USABLE_ISNAN)

#----------------------------------------------------------------------------
# Darwin specific API checks and defines
#--------------------------------------------------------------------
if (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
  TCL_CHECK_FUNCTION_EXISTS(getattrlist HAVE_GETATTRLIST)
  TCL_CHECK_INCLUDE_FILE(copyfile.h HAVE_COPYFILE_H)
  TCL_CHECK_FUNCTION_EXISTS(copyfile HAVE_COPYFILE)
  TCL_CHECK_INCLUDE_FILE(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
  TCL_CHECK_FUNCTION_EXISTS(OSSpinLockLock HAVE_OSSPINLOCKLOCK)
  add_definitions(-DUSE_VFORK=1)
  add_definitions(-DTCL_DEFAULT_ENCODING="utf-8")
  add_definitions(-DTCL_LOAD_FROM_MEMORY=1)
  add_definitions(-DTCL_WIDE_CLICKS=1)
  if (HAVE_AVAILABILITYMACROS_H)

    add_definitions(-DHAVE_AVAILABILITYMACROS_H=1)

    set(WEAK_IMPORT_SRCS "
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
    if (WEAK_IMPORT_WORKING)
      add_definitions(-DHAVE_WEAK_IMPORT=1)
    endif (WEAK_IMPORT_WORKING)

    set(SUSV3_SRCS "
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
    if (SUSV3_WORKING)
      add_definitions(-D_DARWIN_C_SOURCE=1)
    endif (SUSV3_WORKING)

  endif (HAVE_AVAILABILITYMACROS_H)
endif (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

#--------------------------------------------------------------------
#       Check for support of fts functions (readdir replacement)
#--------------------------------------------------------------------
set(FTS_SRCS "
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
if (FTS_WORKING)
  add_definitions(-DHAVE_FTS=1)
endif (FTS_WORKING)

# TODO SC_BLOCKING_STYLE is for older systems, do we still need it?

#------------------------------------------------------------------------
#       Check whether the timezone data is supplied by the OS or has
#       to be installed by Tcl. The default is autodetection, but can
#       be overriden on the configure command line either way.
#------------------------------------------------------------------------
if (TCL_TIMEZONE_DATA)
  if (${TCL_TIMEZONE_DATA} STREQUAL "")
    set(TCL_TIMEZONE_DATA "AUTO" CACHE STRING "Use Tcl's local timezone data")
  endif (${TCL_TIMEZONE_DATA} STREQUAL "")
else (TCL_TIMEZONE_DATA)
  set(TCL_TIMEZONE_DATA "AUTO" CACHE STRING "Use Tcl's local timezone data")
endif (TCL_TIMEZONE_DATA)
if (${TCL_TIMEZONE_DATA} STREQUAL "AUTO")
  set(LOCAL_TZ_DATA OFF)
  find_package(TIMEZONE)
  if (NOT TIMEZONE_FOUND)
    set(LOCAL_TZ_DATA ON)
  endif (NOT TIMEZONE_FOUND)
else (${TCL_TIMEZONE_DATA} STREQUAL "AUTO")
  set(LOCAL_TZ_DATA ${TCL_TIMEZONE_DATA})
endif (${TCL_TIMEZONE_DATA} STREQUAL "AUTO")

#--------------------------------------------------------------------
#       DTrace support
#--------------------------------------------------------------------
# TODO

#--------------------------------------------------------------------
#  Does the C stack grow upwards or downwards? Or cross-compiling?
#--------------------------------------------------------------------
set(C_STACK_SRC "
int StackGrowsUp(int *parent) { int here; return (&here < parent); }
int main (int argc, char *argv[]) { int foo; return StackGrowsUp(&foo); }
")
if (NOT DEFINED STACK_GROWS_UP)
  CHECK_C_SOURCE_RUNS("${C_STACK_SRC}" STACK_GROWS_UP)
endif (NOT DEFINED STACK_GROWS_UP)
if (STACK_GROWS_UP)
  add_definitions(-DTCL_STACK_GROWS_UP=1)
endif (STACK_GROWS_UP)

CHECK_FD_SET_IN_TYPES_D()

#--------------------------------------------------------------------
#       OSX Framework Configuration
#--------------------------------------------------------------------
# TODO

# If we collected any compat sources, pass the flag
if (COMPAT_SRCS OR USE_COMPAT)
  add_definitions(-DUSE_COMPAT=1)
endif (COMPAT_SRCS OR USE_COMPAT)

if (APPLE)
  # Now that all the tests are done, configure the tclConfig.h file:
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unix/tclConfig.h.in ${TCL_BINARY_DIR}/include/tclConfig.h)
endif (APPLE)

#--------------------------------------------------------------------
# At a minimum, this is needed for init.tcl setup
ADD_SUBDIRECTORY(library)

set(TCL_GENERIC_SRCS
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
  generic/tclIORChan.c
  generic/tclIORTrans.c
  generic/tclIOGT.c
  generic/tclIOSock.c
  generic/tclIOUtil.c
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
  generic/tclStringObj.c
  generic/tclStrToD.c
  generic/tclThread.c
  generic/tclThreadAlloc.c
  generic/tclThreadJoin.c
  generic/tclThreadStorage.c
  generic/tclStubInit.c
  generic/tclTimer.c
  generic/tclTrace.c
  generic/tclUtf.c
  generic/tclUtil.c
  generic/tclVar.c
  generic/tclZlib.c
  generic/tclTomMathInterface.c
  )

set(TCL_TEST_SRCS
  generic/tclTest.c
  generic/tclThreadTest.c
  unix/tclUnixTest.c
  unix/tclXtTest.c
  win/tclWinTest.c
  )

if (TCL_TOMMATH)
  set(TCL_GENERIC_SRCS ${TCL_GENERIC_SRCS} generic/tclTomMathInterface.c)
endif (TCL_TOMMATH)

set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_INSTALL_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man") 
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_BINDIR="${CMAKE_INSTALL_PREFIX}/bin")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_SCRDIR="${CMAKE_INSTALL_PREFIX}/scripts")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_INCDIR="${CMAKE_INSTALL_PREFIX}/include")
set_property(SOURCE generic/tclPkgConfig.c APPEND PROPERTY COMPILE_DEFINITIONS CFG_RUNTIME_DOCDIR="${CMAKE_INSTALL_PREFIX}/share/man")


set(TCL_OO_SRCS
  generic/tclOO.c
  generic/tclOOBasic.c
  generic/tclOOCall.c
  generic/tclOODefineCmds.c
  generic/tclOOInfo.c
  generic/tclOOMethod.c
  generic/tclOOStubInit.c
  )


set(TCL_STUB_SRCS
  generic/tclStubLib.c
  generic/tclTomMathStubLib.c
  generic/tclOOStubLib.c
  )

set(TCL_TOMMATH_SRCS
  libtommath/bn_s_mp_reverse.c
  libtommath/bn_s_mp_mul_digs_fast.c
  libtommath/bn_s_mp_sqr_fast.c
  libtommath/bn_mp_add.c
  libtommath/bn_mp_and.c
  libtommath/bn_mp_add_d.c
  libtommath/bn_mp_clamp.c
  libtommath/bn_mp_clear.c
  libtommath/bn_mp_clear_multi.c
  libtommath/bn_mp_cmp.c
  libtommath/bn_mp_cmp_d.c
  libtommath/bn_mp_cmp_mag.c
  libtommath/bn_mp_cnt_lsb.c
  libtommath/bn_mp_copy.c
  libtommath/bn_mp_count_bits.c
  libtommath/bn_mp_div.c
  libtommath/bn_mp_div_d.c
  libtommath/bn_mp_div_2.c
  libtommath/bn_mp_div_2d.c
  libtommath/bn_mp_div_3.c
  libtommath/bn_mp_exch.c
  libtommath/bn_mp_expt_u32.c
  libtommath/bn_mp_grow.c
  libtommath/bn_mp_init.c
  libtommath/bn_mp_init_copy.c
  libtommath/bn_mp_init_multi.c
  libtommath/bn_mp_init_set.c
  libtommath/bn_mp_init_size.c
  libtommath/bn_s_mp_karatsuba_mul.c
  libtommath/bn_s_mp_karatsuba_sqr.c
  libtommath/bn_s_mp_balance_mul.c
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
  libtommath/bn_mp_shrink.c
  libtommath/bn_mp_sqr.c
  libtommath/bn_mp_sqrt.c
  libtommath/bn_mp_sub.c
  libtommath/bn_mp_sub_d.c
  libtommath/bn_mp_signed_rsh.c
  libtommath/bn_mp_to_ubin.c
  libtommath/bn_s_mp_toom_mul.c
  libtommath/bn_s_mp_toom_sqr.c
  libtommath/bn_mp_to_radix.c
  libtommath/bn_mp_ubin_size.c
  libtommath/bn_mp_xor.c
  libtommath/bn_mp_zero.c
  libtommath/bn_s_mp_add.c
  libtommath/bn_s_mp_mul_digs.c
  libtommath/bn_s_mp_sqr.c
  libtommath/bn_s_mp_sub.c
  libtommath/tommath_private.h
  )

set(TCL_OSX_SRCS
  macosx/tclMacOSXBundle.c
  macosx/tclMacOSXFCmd.c
  macosx/tclMacOSXNotify.c
  )

set(TCL_WIN_SRCS
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
  win/tclMainUNICODE.c
  )

set(TCL_UNIX_SRCS
  unix/tclAppInit.c
  unix/tclUnixChan.c
  unix/tclUnixCompat.c
  unix/tclUnixEvent.c
  unix/tclUnixFCmd.c
  unix/tclUnixFile.c
  unix/tclUnixInit.c
  unix/tclUnixNotfy.c
  unix/tclUnixPipe.c
  unix/tclUnixSock.c
  unix/tclUnixThrd.c
  unix/tclUnixTime.c
  )

set(TCL_NOTIFY_SRCS
  unix/tclUnixNotfy.c
  )

set(TCL_SRCS ${TCL_GENERIC_SRCS} ${TCL_OO_SRCS} ${TCL_TOMMATH_SRCS} ${TCL_STUB_SRCS})
if (WIN32)
  set(TCL_SRCS ${TCL_SRCS} ${TCL_WIN_SRCS})
else (WIN32)
  set(TCL_SRCS ${TCL_SRCS} ${TCL_NOTIFY_SRCS} ${TCL_UNIX_SRCS})
  if (APPLE)
    set(TCL_SRCS ${TCL_SRCS} ${TCL_OSX_SRCS} unix/tclLoadDyld.c)
  endif (APPLE)
  if (NOT APPLE)
    set(TCL_SRCS ${TCL_SRCS} unix/tclLoadDl.c)
  endif (NOT APPLE)
endif (WIN32)

if (WIN32)
  set(TCLSH_SRCS win/tclAppInit.c)
else (WIN32)
  set(TCLSH_SRCS unix/tclAppInit.c)
endif (WIN32)

TCL_WIN_FLAGS(TCL_SRCS)
TCL_WIN_FLAGS(TCL_STUB_SRCS)
TCL_WIN_FLAGS(TCLSH_SRCS)

set_property(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set_property(SOURCE win/tclWinInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")
set_property(SOURCE win/tclMainUNICODE.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_UNICODE)
set_property(SOURCE win/tclAppInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_UNICODE)

set_property(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_LIBRARY="${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}tcl${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set_property(SOURCE unix/tclUnixInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_PACKAGE_PATH="${CMAKE_INSTALL_PREFIX}/lib")

set(TCL_INCLUDE_PATH ${TCL_SOURCE_DIR}/generic ${TCL_SOURCE_DIR}/libtommath ${TCL_BINARY_DIR}/include)
if (WIN32)
  set(TCL_INCLUDE_PATH ${TCL_SOURCE_DIR}/win ${TCL_INCLUDE_PATH})
else (WIN32)
  set(TCL_INCLUDE_PATH ${TCL_INCLUDE_PATH} ${TCL_SOURCE_DIR}/unix)
endif (WIN32)

include_directories(
  ${TCL_INCLUDE_PATH}
  ${ZLIB_INCLUDE_DIRS}
  )

if (MINGW)
  set(TCL_WIN_LIBS ${TCL_WIN_LIBS} ws2_32)
endif (MINGW)

if (WIN32)
  set(TCL_WIN_LIBS ${TCL_WIN_LIBS} Netapi32)
endif (WIN32)

if (MSVC)
  set(TCL_SRCS ${TCL_SRCS} win/tcl.rc)
endif (MSVC)

set(TCL_LINK_LIBS
  ${CoreFoundation_LIBRARIES}
  ${TCL_WIN_LIBS}
  ${DL_LIBRARY}
  ${SOCKET_LIBRARY}
  ${GHBN_LIBRARY}
  ${ZLIB_LIBRARIES}
  ${M_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
 )

add_library(tcl ${TCL_SRCS})
target_link_libraries(tcl ${TCL_LINK_LIBS})
install(TARGETS tcl
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
set_target_properties(tcl PROPERTIES VERSION ${TCL_VERSION} SOVERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})
if (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")
  set_target_properties(tcl PROPERTIES LINK_FLAGS "-compatibility_version ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR} -current_version ${TCL_VERSION} -install_name \"$<TARGET_FILE>\" -seg1addr 0xa000000 -sectcreate __TEXT __info_plist \"${TCL_BINARY_DIR}/Tcl-Info.plist\"")
  set(EXTRA_TCLSH_LIBS "-sectcreate __TEXT __info_plist \"${TCL_BINARY_DIR}/Tclsh-Info.plist\"")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tcl-Info.plist.in ${TCL_BINARY_DIR}/Tcl-Info.plist)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macosx/Tclsh-Info.plist.in ${TCL_BINARY_DIR}/Tclsh-Info.plist)
endif (${CMAKE_SYSTEM_NAME} MATCHES "^Darwin$")

add_library(tclstub STATIC ${TCL_STUB_SRCS})
target_link_libraries(tclstub ${TCL_LINK_LIBS})
if (FPIC_COMPILER_FLAG)
  set_target_properties(tclstub PROPERTIES COMPILE_FLAGS "-fPIC")
endif (FPIC_COMPILER_FLAG)
install(TARGETS tclstub
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (MSVC)
  set(TCLSH_SRCS ${TCLSH_SRCS} win/tclsh.rc)
endif (MSVC)

set_property(SOURCE win/tclAppInit.c APPEND PROPERTY COMPILE_DEFINITIONS TCL_UNICODE)
add_executable(tclsh ${TCLSH_SRCS})
target_link_libraries(tclsh tcl ${M_LIBRARY} ${ZLIB_LIBRARIES} ${TCL_THREADS_LIB} ${EXTRA_TCLSH_LIBS})
install(TARGETS tclsh DESTINATION bin)
set_target_properties(tclsh PROPERTIES VERSION ${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR})

set(TCL_HDRS
  generic/tcl.h
  generic/tclDecls.h
  generic/tclPlatDecls.h
  generic/tclTomMath.h
  generic/tclTomMathDecls.h
  generic/tclOO.h
  generic/tclOODecls.h
  )
install(FILES ${TCL_HDRS} DESTINATION include)

# Set up build directory copies of the public headers
foreach(hdrfile ${TCL_HDRS})
  get_filename_component(hf ${hdrfile} NAME)
  if (NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/${hf} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${hdrfile} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${INCLUDE_DIR}/${hf} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif (NOT CMAKE_CONFIGURATION_TYPES)
endforeach(hdrfile ${TCL_HDRS})

mark_as_advanced(TCL_LIBRARIES TCL_CONF_PREFIX TCL_INCLUDE_PATH TCL_INCLUDE_PATH)
mark_as_advanced(TCL_STUB_LIBRARIES TCL_TCLSH TCL_TCLSH_EXECUTABLE)
mark_as_advanced(TCL_TK_CONF_PREFIX TCL_TOMMATH)
mark_as_advanced(TCL_VERSION_MAJOR TCL_VERSION_MINOR TCL_WISH_EXECUTABLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

