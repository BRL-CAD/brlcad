#########################################################################
#
#	BRL-CAD
#	
#	Copyright (c) 1997-2011 United States Government as represented by
#	the U.S. Army Research Laboratory.
#	
#	This library is free software; you can redistribute it and/or
#	modify it under the terms of the GNU Lesser General Public License
#	version 2.1 as published by the Free Software Foundation.
#	
#	This library is distributed in the hope that it will be useful, but
#	WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#	Lesser General Public License for more details.
#	
#	You should have received a copy of the GNU Lesser General Public
#	License along with this file; see the file named COPYING for more
#	information.
#
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

add_definitions(-BUILD_sqlite)

IF(MSVC)
	add_definitions(-DSQLITE_MAX_TRIGGER_DEPTH=100)
	add_definitions(-DSQLITE_ENABLE_COLUMN_METADATA)
ENDIF(MSVC)

add_definitions(-D_REENTRANT=1)

add_definitions(-DSQLITE_THREADSAFE=1)

add_definitions(-DSQLITE_ENABLE_FTS3)
add_definitions(-DSQLITE_ENABLE_RTREE)

INCLUDE(CheckLibraryExists)
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/CMake/ResolveCompilerPaths.cmake)
CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_DLOPEN)
IF(HAVE_DLOPEN)
	RESOLVE_LIBRARIES(DL_LIBRARY "-ldl")
	SET(SQLITE_LIBS ${SQLITE_LIBS} ${DL_LIBRARY})
ENDIF(HAVE_DLOPEN)

add_executable(sqlite3 shell.c sqlite3.c)
target_link_libraries(sqlite3 ${SQLITE_LIBS})
install(TARGETS sqlite3 DESTINATION bin)

# Tcl bindings
ADD_SUBDIRECTORY(tcl)
