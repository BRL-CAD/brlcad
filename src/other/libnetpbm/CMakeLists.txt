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
cmake_minimum_required(VERSION 2.8.8)
if(COMMAND CMAKE_POLICY)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
    CMAKE_POLICY(SET CMP0042 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)

  if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
    CMAKE_POLICY(SET CMP0046 NEW)
    CMAKE_POLICY(SET CMP0053 NEW)
    CMAKE_POLICY(SET CMP0054 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)


# set CMake project name
project(NETBPM)


if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT LIB_DIR)
  set(LIB_DIR lib)
endif(NOT LIB_DIR)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/util
  )

set(NETPBM_VERSION 10.35.98)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/version_new.h ${CMAKE_CURRENT_BINARY_DIR}/version.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/version_new.h)
#string(TIMESTAMP TIME_STAMP)
set(TIME_STAMP "0000")
configure_file(compile.h.in ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h ${CMAKE_CURRENT_BINARY_DIR}/compile.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/compile_new.h)

include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES(fcntl.h HAVE_FCNTL_H)
CHECK_INCLUDE_FILES(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES(sys/types.h HAVE_SYS_STAT_H)

include(CheckTypeSize)
CHECK_TYPE_SIZE(ssize_t HAVE_SSIZE_T)
CHECK_TYPE_SIZE(int64_t HAVE_INT64)
CHECK_TYPE_SIZE(long LONG_SIZE)
CHECK_TYPE_SIZE(int INT_SIZE)
if(${INT_SIZE} GREATER ${LONG_SIZE})
  math(EXPR BITS_PER_WORD "${INT_SIZE} * 8")
else(${INT_SIZE} GREATER ${LONG_SIZE})
  math(EXPR BITS_PER_WORD "${LONG_SIZE} * 8")
endif(${INT_SIZE} GREATER ${LONG_SIZE})

configure_file(pm_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h ${CMAKE_CURRENT_BINARY_DIR}/pm_config.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/pm_config_new.h)

set(NETPBM_SRCS
  libpm.c
  fileio.c
  bitio.c
  colorname.c
  libpbm1.c
  libpbm2.c
  libpbm3.c
  libpbmfont.c
  libpgm1.c
  libpgm2.c
  libppm1.c
  libppm2.c
  libppmcmap.c
  libppmcolor.c
  libppmfuzzy.c
  libppmd.c
  ppmdfont.c
  standardppmdfont.c
  path.c
  libppmfloyd.c
  libpnm1.c
  libpnm2.c
  libpnm3.c
  libpam.c
  libpamread.c
  libpamwrite.c
  libpamn.c
  libpammap.c
  libpamcolor.c
  util/nstring.c
  )

set(NETPBM_HEADERS
  bitio.h
  colorname.h
  pam.h
  pammap.h
  pbm.h
  pbmfont.h
  pgm.h
  pm.h
  pm_gamma.h
  pm_system.h
  pnm.h
  ppm.h
  ppmcmap.h
  ppmfloyd.h
  util/nstring.h
  )

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

if(BUILD_SHARED_LIBS)
  add_library(netpbm SHARED ${NETPBM_SRCS})
  install(TARGETS netpbm
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_SHARED_LIBS)

if(MSVC)
  # msvc does not append 'lib' - do it here to have consistent name
  set_target_properties(netpbm PROPERTIES PREFIX "lib")
  set_target_properties(netpbm PROPERTIES IMPORT_PREFIX "lib")

  set_property(TARGET netpbm APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_EXPORTS")
endif(MSVC)

if(BUILD_STATIC_LIBS)
  add_library(netpbm-static STATIC ${NETPBM_SRCS})
  if(NOT WIN32)
    set_target_properties(netpbm-static PROPERTIES OUTPUT_NAME "netpbm")
  endif(NOT WIN32)
  install(TARGETS netpbm-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BUILD_STATIC_LIBS)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

