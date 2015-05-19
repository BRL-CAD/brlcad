# Copyright (c) 2015 United States Government as represented by
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

# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.4)
if(COMMAND CMAKE_POLICY)
  # libraries linked via full path don't introduce linker search paths
  CMAKE_POLICY(SET CMP0003 NEW)

  # list command doesn't ignore empty elements
  CMAKE_POLICY(SET CMP0007 NEW)

  # prefer files in CMAKE_MODULE_PATH over the cmake module dir
  CMAKE_POLICY(SET CMP0017 OLD)

  # allow relative paths in INCLUDE_DIRECTORIES
  if ("${CMAKE_VERSION}" VERSION_EQUAL 2.8.12 OR "${CMAKE_VERSION}" VERSION_GREATER 2.8.12)
    CMAKE_POLICY(SET CMP0021 OLD)
  endif ("${CMAKE_VERSION}" VERSION_EQUAL 2.8.12 OR "${CMAKE_VERSION}" VERSION_GREATER 2.8.12)

  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
    CMAKE_POLICY(SET CMP0042 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)

  if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
    CMAKE_POLICY(SET CMP0046 NEW)
    CMAKE_POLICY(SET CMP0053 OLD)
    CMAKE_POLICY(SET CMP0054 NEW)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
endif(COMMAND CMAKE_POLICY)

project(DOM2DOX)

set(DOM2DOX_CMAKE_DIR "${DOM2DOX_SOURCE_DIR}/CMake")
if(DOM2DOX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${DOM2DOX_CMAKE_DIR}")
else(DOM2DOX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${DOM2DOX_CMAKE_DIR};${CMAKE_MODULE_PATH}")
endif(DOM2DOX_IS_SUBBUILD)

find_package(LEMON)
find_package(RE2C)
find_package(PERPLEX)

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/PERPLEX_Util.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/LEMON_Util.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/RE2C_Util.cmake)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR})


set(DOC_CONVERTER dom2dox)

PERPLEX_TARGET(${DOC_CONVERTER}_scanner ${DOC_CONVERTER}_scanner.perplex
  ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_scanner.c
  ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_scanner.h)
LEMON_TARGET(${DOC_CONVERTER}_parser ${DOC_CONVERTER}_parser.lemon
  ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_parser.c
  ${CMAKE_CURRENT_BINARY_DIR}/${DOC_CONVERTER}_parser.h)
ADD_PERPLEX_LEMON_DEPENDENCY(${DOC_CONVERTER}_scanner ${DOC_CONVERTER}_parser)

add_executable(${DOC_CONVERTER} ${DOC_CONVERTER}.c ${DOC_CONVERTER}_scanner.c ${DOC_CONVERTER}_parser.c)
target_link_libraries(${DOC_CONVERTER} libbu)

# TODO - The logic below belongs in BRL-CAD's doc directory

#set(CONVERSION_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
#foreach(ON_source ${ON_sources})
#  add_custom_command(OUTPUT "${ON_source}" DEPENDS ${DOC_CONVERTER}
#    COMMAND cmake ARGS -E make_directory "${CONVERSION_DIR}"
#    COMMAND ${DOC_CONVERTER} ARGS "${OPENNURBS_SOURCE_ROOT}/${ON_source}" "${CONVERSION_DIR}/${ON_source}")
#endforeach(ON_source)

#set(DOC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
#add_custom_target(ON_DOC ALL DEPENDS ${ON_sources}
#  COMMAND cmake -E make_directory "${DOC_OUTPUT_DIR}"
#  COMMAND cmake -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile" "${DOC_OUTPUT_DIR}/Doxyfile"
#  COMMAND cmake -E chdir "${DOC_OUTPUT_DIR}" doxygen Doxyfile
#  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
