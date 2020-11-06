# Copyright (c) 2010-2016 United States Government as represented by
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

# ******************************************************************
# ***                stepcode CMakeLists.txt                     ***
# ******************************************************************

cmake_minimum_required(VERSION 3.12)

project(STEPCODE)

# STEPCODE version
set(STEPCODE_VERSION_MAJOR 0)
set(STEPCODE_VERSION_MINOR 8-dev)
set(STEPCODE_VERSION ${STEPCODE_VERSION_MAJOR}.${STEPCODE_VERSION_MINOR})

# STEPCODE ABI version. STEPCODE_ABI_SOVERSION should be incremented
# for each release introducing API incompatibilities
set(STEPCODE_ABI_SOVERSION 3)
set(STEPCODE_ABI_VERSION ${STEPCODE_ABI_SOVERSION}.0.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

include(Path_Setup)

# Require C/C++11
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckIncludeFiles)
check_include_files(ndir.h HAVE_NDIR_H)
check_include_files(stdarg.h HAVE_STDARG_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/param.h HAVE_SYS_PARAM_H)
check_include_files(sysent.h HAVE_SYSENT_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(dirent.h HAVE_DIRENT_H)
check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(process.h HAVE_PROCESS_H)
check_include_files(io.h HAVE_IO_H)

include(CheckSymbolExists)
check_symbol_exists(abs "stdlib.h" HAVE_ABS)
check_symbol_exists(memcpy "string.h" HAVE_MEMCPY)
check_symbol_exists(memmove "string.h" HAVE_MEMMOVE)
check_symbol_exists(getopt "getopt.h" HAVE_GETOPT)
check_symbol_exists(snprintf "stdio.h" HAVE_SNPRINTF)
check_symbol_exists(vsnprintf "stdio.h" HAVE_VSNPRINTF)

include(CheckTypeSize)
check_type_size("ssize_t" SSIZE_T)

configure_file(include/stepcode/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/stepcode/config.h)

# Common to all libraries/executables
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/stepcode
  ${CMAKE_CURRENT_BINARY_DIR}/include/stepcode
  )

add_subdirectory(src)

# Get install rules for headers and data files
add_subdirectory(include/stepcode)
add_subdirectory(data)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

