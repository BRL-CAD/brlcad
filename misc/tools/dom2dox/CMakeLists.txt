# Copyright (c) 2015-2022 United States Government as represented by
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

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

project(DOM2DOX)

set(DOM2DOX_CMAKE_DIR "${DOM2DOX_SOURCE_DIR}/CMake")
if(DOM2DOX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${DOM2DOX_CMAKE_DIR}")
else(DOM2DOX_IS_SUBBUILD)
  set(CMAKE_MODULE_PATH "${DOM2DOX_CMAKE_DIR};${CMAKE_MODULE_PATH}")
endif(DOM2DOX_IS_SUBBUILD)

if(NOT DOM2DOX_IS_SUBBUILD)
  find_package(LEMON)
  find_package(RE2C)
  find_package(PERPLEX)
endif(NOT DOM2DOX_IS_SUBBUILD)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_BINARY_DIR}/include
  ${TCL_INCLUDE_PATH}
  )

PERPLEX_TARGET(dom2dox_scanner dom2dox_scanner.perplex)
LEMON_TARGET(dom2dox_parser dom2dox_parser.lemon)
ADD_PERPLEX_LEMON_DEPENDENCY(dom2dox_scanner dom2dox_parser)

set(dom2dox_srcs
  dom2dox.c
  ${PERPLEX_dom2dox_scanner_SRC}
  ${LEMON_dom2dox_parser_SRC}
)
add_executable(dom2dox ${dom2dox_srcs})
target_link_libraries(dom2dox libbu)
set_property(TARGET dom2dox APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_dom2dox_scanner_INCLUDE_DIR}")
set_property(TARGET dom2dox APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_dom2dox_parser_INCLUDE_DIR}")

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)
if(NOERROR_FLAG)
  set_property(TARGET dom2dox APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
endif(NOERROR_FLAG)

# TODO - The logic below belongs in BRL-CAD's doc directory

#set(CONVERSION_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
#foreach(ON_source ${ON_sources})
#  add_custom_command(OUTPUT "${ON_source}" DEPENDS dom2dox
#    COMMAND cmake ARGS -E make_directory "${CONVERSION_DIR}"
#    COMMAND dom2dox ARGS "${OPENNURBS_SOURCE_ROOT}/${ON_source}" "${CONVERSION_DIR}/${ON_source}")
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
