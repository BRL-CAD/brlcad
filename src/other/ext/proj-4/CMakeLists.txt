#################################################################################
#
# This file is part of CMake configuration for PROJ4 library (inspired from SOCI
# CMake,  Copyright (C) 2009-2010 Mateusz Loskot <mateusz@loskot.net> )
#
# Copyright (C) 2011 Nicolas David <nicolas.david@ign.fr>
# Distributed under the MIT license
#
#################################################################################
# General settings
#################################################################################
cmake_minimum_required(VERSION 3.12)

# proj4 is an ANSI C project
project(PROJ4 C)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)


# OpenBSD has its own naming conventions.  Set a platform variable based on
# the OS name so we can test for it succinctly.
if ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")
  set(OPENBSD ON)
endif ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")

#################################################################################
set(PROJ_VERSION_MAJOR 4)
set(PROJ_VERSION_MINOR 9)
set(PROJ_VERSION_PATCH 3)
if (OPENBSD)
  set(PROJ_VERSION ${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR})
else (OPENBSD)
  set(PROJ_VERSION ${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_PATCH})
  set(PROJ_API_VERSION "12")
  set(PROJ_BUILD_VERSION "${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_API_VERSION}")
endif (OPENBSD)
set(VERSION ${PROJ_VERSION})
if(WIN32)
  set(PROJ_ABI_VERSION "${PROJ_VERSION_MAJOR}_${PROJ_VERSION_MINOR}")
endif(WIN32)

#################################################################################
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT LIB_DIR)
  if(MSVC)
    set(LIB_DIR bin)
  else(MSVC)
    set(LIB_DIR lib)
  endif(MSVC)
endif(NOT LIB_DIR)
if(NOT DATA_DIR)
  set(DATA_DIR share)
endif(NOT DATA_DIR)
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

if (NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS OFF)
endif (NOT DEFINED BUILD_STATIC_LIBS)

#################################################################################
# From SociConfig.cmake - CMake build configuration of SOCI library
# Copyright (C) 2010 Mateusz Loskot <mateusz@loskot.net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

include (CheckIncludeFiles)
include (CheckLibraryExists)
include (CheckFunctionExists)

# check needed include file
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(jni.h HAVE_JNI_H)
check_include_files(memory.h HAVE_MEMORY_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(unistd.h HAVE_UNISTD_H)
set(STDC_HEADERS 1)
check_function_exists(localeconv HAVE_LOCALECONV)

# check libm need on unix
check_library_exists(m ceil "" HAVE_LIBM)
if(HAVE_LIBM)
  set(M_LIBRARY m)
endif(HAVE_LIBM)

#################################################################################
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
option(USE_THREAD "Build libproj with thread/mutex support " ON)
if(NOT USE_THREAD)
  add_definitions( -DMUTEX_stub)
endif(NOT USE_THREAD)
find_package(Threads QUIET)
if(USE_THREAD AND NOT Threads_FOUND)
  set(USE_THREAD OFF)
  message(WARNING "USE_THREAD enabled, but no threading support found - disabling")
  add_definitions( -DMUTEX_stub)
else()
  if(USE_THREAD AND Threads_FOUND AND CMAKE_USE_WIN32_THREADS_INIT )
    add_definitions( -DMUTEX_win32)
  endif(USE_THREAD AND Threads_FOUND AND CMAKE_USE_WIN32_THREADS_INIT )
  if(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT )
    add_definitions( -DMUTEX_pthread)
  endif(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT )
endif(USE_THREAD AND NOT Threads_FOUND)

include(CheckIncludeFiles)
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(PTHREAD_MUTEX_RECURSIVE pthread.h HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
if (HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
  add_definitions(-DHAVE_PTHREAD_MUTEX_RECURSIVE=1)
endif()

#################################################################################

configure_file(proj_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/proj_config.h)

add_subdirectory(nad)
add_subdirectory(src)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

