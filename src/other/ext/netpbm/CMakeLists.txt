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
# ***                libnetpbm CMakeLists.txt                     ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# set CMake project name
project(NETPBM)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

if (NOT BIN_DIR)
  set(BIN_DIR bin)
endif (NOT BIN_DIR)
if (NOT LIB_DIR)
  set(LIB_DIR lib)
endif (NOT LIB_DIR)
if (NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT INCLUDE_DIR)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/netpbm
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/util
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/netpbm
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/util/netpbm
  )

set(NETPBM_VERSION 10.73.42)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/netpbm)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/version_new.h ${CMAKE_CURRENT_BINARY_DIR}/netpbm/version.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/version_new.h)
#string(TIMESTAMP TIME_STAMP)
set(TIME_STAMP "0000")
configure_file(compile.h.in ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h ${CMAKE_CURRENT_BINARY_DIR}/compile.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h)

include(CheckIncludeFiles)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/types.h HAVE_SYS_STAT_H)

include(CheckTypeSize)
check_type_size(ssize_t HAVE_SSIZE_T)
check_type_size(int64_t HAVE_INT64)
check_type_size(long LONG_SIZE)
check_type_size(int INT_SIZE)
if(${INT_SIZE} GREATER ${LONG_SIZE})
  math(EXPR BITS_PER_WORD "${INT_SIZE} * 8")
else(${INT_SIZE} GREATER ${LONG_SIZE})
  math(EXPR BITS_PER_WORD "${LONG_SIZE} * 8")
endif(${INT_SIZE} GREATER ${LONG_SIZE})

configure_file(pm_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h ${CMAKE_CURRENT_BINARY_DIR}/netpbm/pm_config.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h)

set(NETPBM_SRCS
  lib/colorname.c
  lib/fileio.c
  lib/libpam.c
  lib/libpamcolor.c
  lib/libpammap.c
  lib/libpamn.c
  lib/libpamread.c
  lib/libpamwrite.c
  lib/libpbm1.c
  lib/libpbm2.c
  lib/libpbm3.c
  lib/libpbmfont.c
  lib/libpgm1.c
  lib/libpgm2.c
  lib/libpm.c
  lib/libpnm1.c
  lib/libpnm2.c
  lib/libpnm3.c
  lib/libppm1.c
  lib/libppm2.c
  lib/libppmcmap.c
  lib/libppmcolor.c
  lib/libppmd.c
  lib/libppmfloyd.c
  lib/libppmfuzzy.c
  lib/path.c
  lib/pmfileio.cxx
  lib/ppmdfont.c
  lib/standardppmdfont.c
  lib/util/bitio.c
  lib/util/mallocvar.c
  lib/util/nstring.c
  lib/util/vasprintf.c
  )

set(NETPBM_HEADERS
  lib/netpbm/colorname.h
  lib/netpbm/pam.h
  lib/netpbm/pammap.h
  lib/netpbm/pbm.h
  lib/netpbm/pbmfont.h
  lib/netpbm/pgm.h
  lib/netpbm/pm.h
  lib/netpbm/pm_gamma.h
  lib/netpbm/pnm.h
  lib/netpbm/ppm.h
  lib/netpbm/ppmcmap.h
  lib/netpbm/ppmfloyd.h
  lib/util/netpbm/bitio.h
  ${CMAKE_CURRENT_BINARY_DIR}/netpbm/pm_config.h
  )

install (FILES ${NETPBM_HEADERS} DESTINATION ${INCLUDE_DIR}/netpbm)


if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

if (BUILD_SHARED_LIBS)
  add_library(netpbm SHARED ${NETPBM_SRCS})
  install(TARGETS netpbm
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_SHARED_LIBS)

if(MSVC)
  set_property(TARGET netpbm APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_EXPORTS")
endif(MSVC)

if (BUILD_STATIC_LIBS)
  add_library(netpbm-static STATIC ${NETPBM_SRCS})
  if (NOT MSVC)
    set_target_properties(netpbm-static PROPERTIES OUTPUT_NAME "netpbm")
  endif (NOT MSVC)
  install(TARGETS netpbm-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

