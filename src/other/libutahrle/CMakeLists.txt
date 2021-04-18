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

# *******************************************************************
# ***                libutahrle CMakeLists.txt                     ***
# *******************************************************************

cmake_minimum_required(VERSION 3.12)

project(UTAHRLE)


if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)
if(NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS ON)
endif(NOT DEFINED BUILD_STATIC_LIBS)

if(NOT WIN32)
  include(CheckLibraryExists)
  check_library_exists(m cos "" HAVE_M_LIBRARY)
  if(HAVE_M_LIBRARY)
    set(M_LIBRARY m)
  endif(HAVE_M_LIBRARY)
endif(NOT WIN32)

include(CheckIncludeFiles)
check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
  add_definitions(-DHAVE_UNISTD_H=1)
endif(HAVE_UNISTD_H)
check_include_files(sys/wait.h HAVE_SYS_WAIT_H)
if(HAVE_SYS_WAIT_H)
  add_definitions(-DHAVE_SYS_WAIT_H=1)
endif(HAVE_SYS_WAIT_H)

include_directories(
  ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
  )

if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)
if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

if(UTAHRLE_HAVE_COMMON_H)
  add_definitions(-DHAVE_COMMON_H=1)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
endif(UTAHRLE_HAVE_COMMON_H)

set(LIBUTAHRLE_PUBLIC_HDRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/rle.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/rle_code.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/rle_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/rle_put.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/rle_raw.h
  )

set(LIBUTAHRLE_PRIVATE_HDRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/colorquant.h
  )

set(LIBUTAHRLE_SOURCES
  Runput.c
  buildmap.c
  cmd_name.c
  colorquant.c
  dither.c
  float_to_exp.c
  hilbert.c
  inv_cmap.c
  rle_addhist.c
  rle_cp.c
  rle_error.c
  rle_getcom.c
  rle_getraw.c
  rle_getrow.c
  rle_getskip.c
  rle_global.c
  rle_hdr.c
  rle_open_f.c
  rle_putcom.c
  rle_putraw.c
  rle_putrow.c
  rle_raw_alc.c
  rle_rawrow.c
  rle_row_alc.c
  scanargs.c
  vaxshort.c
  )

if(MSVC)
  add_definitions(
    -DNO_DECLARE_MALLOC
    -DNEED_BSTRING
    -DNO_OPEN_PIPES
    )
endif(MSVC)

if(BUILD_SHARED_LIBS)
  add_library(utahrle SHARED ${LIBUTAHRLE_SOURCES})
  if(MSVC)
    set_property(TARGET utahrle APPEND PROPERTY COMPILE_DEFINITIONS "RLE_DLL_EXPORTS")
  endif(MSVC)
  target_link_libraries(utahrle ${M_LIBRARY})
  set_target_properties(utahrle PROPERTIES VERSION 19.0.1 SOVERSION 19)
  set_target_properties(utahrle PROPERTIES FOLDER "Third Party Libraries")
  install(TARGETS utahrle
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
  add_library(utahrle-static STATIC ${LIBUTAHRLE_SOURCES})
  if(MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(utahrle-static PROPERTIES PREFIX "lib")
  else(MSVC)
    set_target_properties(utahrle-static PROPERTIES OUTPUT_NAME "utahrle")
  endif(MSVC)
  target_link_libraries(utahrle-static ${M_LIBRARY})
  set_target_properties(utahrle-static PROPERTIES FOLDER "Third Party Libraries")
  install(TARGETS utahrle-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)

if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
  install(FILES ${LIBUTAHRLE_PUBLIC_HDRS} DESTINATION include)
endif(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

