#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2022 United States Government as represented by
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
# Build file for sqlite (above license applies to only this file - lmdb is
# covered by its own license.)
#
# SQLite is an in-process library that implements a self-contained, serverless,
# zero-configuration, transactional SQL database engine.
# https://www.sqlite.org/index.html
###

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(SQLITE)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT DEFINED BIN_DIR)
  set (BIN_DIR bin)
endif (NOT DEFINED BIN_DIR)

if (NOT DEFINED LIB_DIR)
  set (LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

if (NOT DEFINED INCLUDE_DIR)
  set (INCLUDE_DIR include)
endif (NOT DEFINED INCLUDE_DIR)

find_package(Threads)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(M_LIBRARY m)
endif (HAVE_M_LIBRARY)

# https://www.sqlite.org/howtocompile.html
add_definitions(-DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_RTREE=1)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(libsqlite3 SHARED sqlite3.c)
set_target_properties(libsqlite3 PROPERTIES PREFIX "")
set_target_properties(libsqlite3 PROPERTIES VERSION 3.41.2 SOVERSION 3)
target_link_libraries(libsqlite3 PRIVATE Threads::Threads ${M_LIBRARY})
set_property(TARGET libsqlite3 APPEND PROPERTY COMPILE_DEFINITIONS "SQLITE_DLL_EXPORTS")
set_property(TARGET libsqlite3 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SQLITE_DLL_IMPORTS")
install(TARGETS libsqlite3
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

add_executable(sqlite3 sqlite3.c shell.c)
target_link_libraries(sqlite3 Threads::Threads ${M_LIBRARY})
install(TARGETS sqlite3
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

install(FILES sqlite3.h sqlite3ext.h DESTINATION ${INCLUDE_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
