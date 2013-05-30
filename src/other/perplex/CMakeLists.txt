#                  C M A K E L I S T S . T X T
#
# Perplex - re2c scanner input generator
#
# Copyright (c) 2012 United States Government as represented by
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
###
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

PROJECT(PERPLEX)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(PERPLEX_CMAKE_DIR "${PERPLEX_SOURCE_DIR}/CMake")
if(PERPLEX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PERPLEX_CMAKE_DIR}")
else(PERPLEX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${PERPLEX_CMAKE_DIR};${CMAKE_MODULE_PATH}")
endif(PERPLEX_IS_SUBBUILD)

FIND_PACKAGE(LEMON)
FIND_PACKAGE(RE2C)

if(NOT LEMON_EXECUTABLE OR NOT RE2C_EXECUTABLE)
  message(FATAL_ERROR "Perplex requires the LEMON Parser Generator and the re2c scanner generator.")
endif(NOT LEMON_EXECUTABLE OR NOT RE2C_EXECUTABLE)

if(NOT COMMAND LEMON_TARGET)
  include(${PERPLEX_CMAKE_DIR}/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)
if(NOT COMMAND RE2C_TARGET)
  include(${PERPLEX_CMAKE_DIR}/RE2C_Util.cmake)
endif(NOT COMMAND RE2C_TARGET)

RE2C_TARGET(PERPLEX scanner.re ${CMAKE_CURRENT_BINARY_DIR}/scanner.c COMPILE_FLAGS "-c")
LEMON_TARGET(PERPLEX parser.y parser.c parser.h)
ADD_RE2C_LEMON_DEPENDENCY(PERPLEX PERPLEX)

set(perplex_sources
  ${LEMON_PERPLEX_OUTPUT_SOURCE}
  ${RE2C_PERPLEX_OUTPUTS}
  mbo_getopt.cpp
  perplex.cpp
  )

add_executable(perplex ${perplex_sources})

# Need to link against stl library with some compilers.
target_link_libraries(perplex "${STDCXX_LIBRARIES}")

# Install
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)
if(NOT PERPLEX_SKIP_INSTALL)
  install(TARGETS perplex DESTINATION ${BIN_DIR})
endif(NOT PERPLEX_SKIP_INSTALL)

# Handle Perplex template
if(NOT DATA_DIR)
  set(DATA_DIR share)
endif(NOT DATA_DIR)

if(NOT CMAKE_CONFIGURATION_TYPES)
  set(PERPLEX_TEMPLATE_DIR ${CMAKE_BINARY_DIR}/${DATA_DIR}/perplex)
else(NOT CMAKE_CONFIGURATION_TYPES)
  set(PERPLEX_TEMPLATE_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DATA_DIR}/perplex)
endif(NOT CMAKE_CONFIGURATION_TYPES)
add_custom_command(
  OUTPUT ${PERPLEX_TEMPLATE_DIR}/perplex_template.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c ${PERPLEX_TEMPLATE_DIR}/perplex_template.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c
  )
add_custom_target(perplex_template_cp ALL DEPENDS ${PERPLEX_TEMPLATE_DIR}/perplex_template.c)
add_dependencies(perplex perplex_template_cp)

if(NOT PERPLEX_SKIP_INSTALL)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/perplex_template.c DESTINATION ${DATA_DIR})
endif(NOT PERPLEX_SKIP_INSTALL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
