#                   M I M E _ E X T E N S I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
cmake_minimum_required(VERSION 3.26)
if(NOT SOURCE_DIR OR NOT TEST_ROOT)
  message(FATAL_ERROR "SOURCE_DIR and TEST_ROOT are required")
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/source/misc")
file(WRITE "${TEST_ROOT}/source/misc/mime.types" "image/jpeg jpg\nimage/png png\n")
file(WRITE "${TEST_ROOT}/source/misc/mime_cad.types" "")
file(WRITE "${TEST_ROOT}/source/misc/mime_extensions.types" "")
file(WRITE "${TEST_ROOT}/generate.cmake" [=[
cmake_minimum_required(VERSION 3.26)
set(BRLCAD_SOURCE_DIR "${TEST_ROOT}/source")
set(CMAKE_BINARY_DIR "${TEST_ROOT}/build-${STAGE}")
set(CMAKE_CURRENT_BINARY_DIR "${CMAKE_BINARY_DIR}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/bu")
function(distclean)
endfunction()
include("${SOURCE_DIR}/src/libbu/mime.cmake")
]=])

function(generate stage expect_failure)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SOURCE_DIR}"
    "-DTEST_ROOT=${TEST_ROOT}" "-DSTAGE=${stage}" -P "${TEST_ROOT}/generate.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
  if(expect_failure)
    if(result EQUAL 0)
      message(FATAL_ERROR "Invalid MIME extension registry was accepted: ${stage}")
    endif()
  elseif(NOT result EQUAL 0)
    message(FATAL_ERROR "MIME generation failed: ${output}")
  endif()
endfunction()

generate(original FALSE)
file(READ "${TEST_ROOT}/build-original/include/bu/mime_types.h" original)
# A semicolon in a comment must not become multiple arguments to if().
file(WRITE "${TEST_ROOT}/source/misc/mime_extensions.types" "# additional formats; append only\nimage/exr exr\n")
generate(extended FALSE)
file(READ "${TEST_ROOT}/build-extended/include/bu/mime_types.h" extended)
if(NOT extended MATCHES "BU_MIME_IMAGE_UNKNOWN,[\n ]+BU_MIME_IMAGE_EXR,")
  message(FATAL_ERROR "The new format renumbered a historical image identifier")
endif()
string(REPLACE "\n    BU_MIME_IMAGE_EXR," "" restored "${extended}")
if(NOT restored STREQUAL original)
  message(FATAL_ERROR "Historical MIME declarations changed")
endif()
file(APPEND "${TEST_ROOT}/source/misc/mime_extensions.types" "image/alpha abc\n")
generate(second FALSE)
file(READ "${TEST_ROOT}/build-second/include/bu/mime_types.h" second)
if(NOT second MATCHES "BU_MIME_IMAGE_EXR,[\n ]+BU_MIME_IMAGE_ALPHA,")
  message(FATAL_ERROR "Later formats must preserve earlier extension identifiers")
endif()
file(APPEND "${TEST_ROOT}/source/misc/mime_extensions.types" "image/exr duplicate\n")
generate(duplicate TRUE)
file(WRITE "${TEST_ROOT}/source/misc/mime_extensions.types" "model/new new\n")
generate(new_context TRUE)
file(REMOVE_RECURSE "${TEST_ROOT}")
