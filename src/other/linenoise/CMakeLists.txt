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
# Build file for linenoise (above license applies to only this file - linenoise
# is covered by its own license.)
#
# linenoise fork from https://github.com/msteveb/linenoise
###

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# Set CMake project name
project(LN)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (NOT DEFINED BIN_DIR)
  set (BIN_DIR bin)
endif (NOT DEFINED BIN_DIR)

if (NOT DEFINED LIB_DIR)
  set (LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

if (NOT DEFINED INCLUDE_DIR)
  set (INCLUDE_DIR include)
endif (NOT DEFINED INCLUDE_DIR)

set(LN_SRCS
  utf8.c
  linenoise.c
  stringbuf.c
  )

add_library(linenoise SHARED ${LN_SRCS})
set_property(TARGET linenoise APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_EXPORTS")
set_property(TARGET linenoise APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")
install(TARGETS linenoise
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

set(LN_HDRS
  linenoise.h
  stringbuf.h
  utf8.h
  )
install(FILES ${LN_HDRS} DESTINATION ${INCLUDE_DIR}/linenoise)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
