# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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

cmake_minimum_required(VERSION 2.8)

PROJECT(sqlite)

find_package(Threads REQUIRED)
SET(SQLITE_LIBS ${CMAKE_THREAD_LIBS_INIT})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

IF(NOT LIB_DIR)
  IF(NOT WIN32)
    SET(LIB_DIR lib)
  ELSE(NOT WIN32)
    SET(LIB_DIR bin)
  ENDIF(NOT WIN32)
ENDIF(NOT LIB_DIR)

add_definitions(-DBUILD_sqlite)

IF(MSVC)
  add_definitions(-DSQLITE_MAX_TRIGGER_DEPTH=100)
  add_definitions(-DSQLITE_ENABLE_COLUMN_METADATA)
ENDIF(MSVC)

add_definitions(-D_REENTRANT=1)

add_definitions(-DSQLITE_THREADSAFE=1)

add_definitions(-DSQLITE_ENABLE_FTS3)
add_definitions(-DSQLITE_ENABLE_RTREE)

INCLUDE(CheckLibraryExists)
CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_DL_LIBRARY)
IF(HAVE_DL_LIBRARY)
  set(DL_LIBRARY "dl")
  SET(SQLITE_LIBS ${SQLITE_LIBS} ${DL_LIBRARY})
ENDIF(HAVE_DL_LIBRARY)

add_executable(sqlite3 shell.c sqlite3.c)
target_link_libraries(sqlite3 ${SQLITE_LIBS})
install(TARGETS sqlite3 DESTINATION bin)

# Tcl bindings
ADD_SUBDIRECTORY(tcl)
