#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2020 United States Government as represented by
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
# Build file for libregex (above license applies to only this file -
# libregex is convered by its own license.)
###
project(REGEX)

cmake_minimum_required(VERSION 3.12)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# OpenBSD has its own naming conventions.  Set a platform variable based on
# the OS name so we can test for it succinctly.
if ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")
  set(OPENBSD ON)
endif ("${CMAKE_SYSTEM}" MATCHES ".*OpenBSD.*")

if (NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif (NOT DEFINED BIN_DIR)

if (NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

if (NOT DEFINED INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT DEFINED INCLUDE_DIR)

option(ENABLE_REGEX_PREFIX "Add regex_ prefix to all library functions." OFF)
if (DEFINED REGEX_PREFIX_STR)
  set(ENABLE_REGEX_PREFIX ON CACHE BOOL "Ensure prefix is set" FORCE)
endif (DEFINED REGEX_PREFIX_STR)
if (ENABLE_REGEX_PREFIX)
  set(REGEX_PREFIX 1)
endif (ENABLE_REGEX_PREFIX)
mark_as_advanced(ENABLE_REGEX_PREFIX)
mark_as_advanced(REGEX_PREFIX_STR)

configure_file(regex.h.in ${CMAKE_CURRENT_BINARY_DIR}/regex.h @ONLY)

set (REGEX_HDRS
  ${CMAKE_CURRENT_BINARY_DIR}/regex.h
  )

set (REGEX_SOURCES
  regcomp.c
  regerror.c
  regexec.c
  regfree.c
  )

add_library(regex SHARED ${REGEX_SOURCES})
set_target_properties(regex PROPERTIES OUTPUT_NAME regex_brl)
if (OPENBSD)
  set_target_properties(regex PROPERTIES VERSION 1.0)
else (OPENBSD)
  set_target_properties(regex PROPERTIES VERSION 1.0.4 SOVERSION 1)
endif (OPENBSD)
if (MSVC)
  set_property(TARGET regex APPEND PROPERTY COMPILE_DEFINITIONS "REGEX_DLL_EXPORTS")
endif (MSVC)
install(TARGETS regex
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(regex-static STATIC ${REGEX_SOURCES})
  if (MSVC)
    set_target_properties(regex-static PROPERTIES OUTPUT_NAME "regex_brl-static")
  else (MSVC)
    set_target_properties(regex-static PROPERTIES OUTPUT_NAME "regex_brl")
  endif (MSVC)
  install(TARGETS regex-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

if (NOT SKIP_INSTALL_HEADERS)
  install(FILES ${REGEX_HDRS} DESTINATION ${INCLUDE_DIR})
endif (NOT SKIP_INSTALL_HEADERS)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

