# T E S T _ R T _ C O M M A N D _ S E L E C T I O N . C M A K E
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
if(NOT RT OR NOT DBGEN OR NOT TEST_DIR)
  message(FATAL_ERROR "RT, DBGEN, and TEST_DIR are required")
endif()
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/cache")
set(ENV{BU_DIR_CACHE} "${TEST_DIR}/cache")
set(DB "${TEST_DIR}/scene.g")
set(LOG "${TEST_DIR}/commands.log")
function(run)
  cmake_parse_arguments(PARSE_ARGV 0 options "FAIL" "INPUT_FILE" "")
  set(input)
  if(DEFINED options_INPUT_FILE)
    list(APPEND input INPUT_FILE "${options_INPUT_FILE}")
  endif()
  execute_process(COMMAND ${options_UNPARSED_ARGUMENTS} ${input}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output TIMEOUT 30)
  file(APPEND "${LOG}" "${options_UNPARSED_ARGUMENTS}\n${output}\n")
  if(NOT "${result}" MATCHES "^[0-9]+$" OR
      (options_FAIL AND result EQUAL 0) OR
      (NOT options_FAIL AND NOT result EQUAL 0))
    message(FATAL_ERROR "Unexpected command result ${result}: ${options_UNPARSED_ARGUMENTS}\n${output}")
  endif()
endfunction()
run("${DBGEN}" "${DB}")
set(POSE "viewsize 300; eye_pt 0 0 212.13203435596427; orientation 0 0 0 1")
set(RENDER_ARGS -P 1 -B -w 32 -n 32 -C 0/0/0 -p 0
  -c "viewsize 300" -c "eye_pt 0 0 212.13203435596427" -c "orientation 0 0 0 1")
foreach(object IN ITEMS front.r back.r cube.r sphere.r occluded.g)
  run("${RT}" ${RENDER_ARGS} -o "${TEST_DIR}/${object}.pix" "${DB}" "${object}")
endforeach()
function(render_stream name stream)
  set(input "${TEST_DIR}/${name}.view")
  file(WRITE "${input}" "${POSE}; ${stream}\n")
  run(INPUT_FILE "${input}" "${RT}" ${RENDER_ARGS} -M
    -o "${TEST_DIR}/${name}.pix" "${DB}" ${ARGN})
endfunction()
function(matches actual reference)
  file(SHA256 "${TEST_DIR}/${actual}" actual_hash)
  file(SHA256 "${TEST_DIR}/${reference}.pix" reference_hash)
  if(NOT actual_hash STREQUAL reference_hash)
    message(FATAL_ERROR "Selection pixels differ: ${actual} versus ${reference}")
  endif()
endfunction()
run("${RT}" ${RENDER_ARGS} -o "${TEST_DIR}/default.pix" "${DB}")
matches(default.pix sphere.r)
render_stream(default_edits "start 0; end; start 1; draw back.r; erase back.r; end;")
matches(default_edits.pix sphere.r)
matches(default_edits.pix.1 sphere.r)
render_stream(draw "start 0; draw front.r back.r front.r; prep; end;")
matches(draw.pix occluded.g)
render_stream(add "start 0; draw back.r; end;" front.r)
matches(add.pix occluded.g)
file(WRITE "${TEST_DIR}/objects.txt" "front.r\nback.r\ncube.r\n")
file(WRITE "${TEST_DIR}/object-file.view" "${POSE}; start 0; erase front.r back.r; end;\n")
run(INPUT_FILE "${TEST_DIR}/object-file.view" "${RT}" ${RENDER_ARGS}
  -I "${TEST_DIR}/objects.txt" -M -o "${TEST_DIR}/object-file.pix" "${DB}")
matches(object-file.pix cube.r)
render_stream(erase "start 0; erase front.r back.r; end;" front.r back.r cube.r)
matches(erase.pix cube.r)
render_stream(duplicates "start 0; erase front.r; end;" front.r front.r cube.r)
matches(duplicates.pix cube.r)
render_stream(replace
  "start 0; tree back.r; end; start 1; tree front.r; end; start 2; tree front.r back.r; end;" occluded.g)
matches(replace.pix back.r)
matches(replace.pix.1 front.r)
matches(replace.pix.2 occluded.g)
render_stream(articulation
  "start 0; anim /back.r matrix rmul translate 120 0 0; draw cube.r; erase cube.r; end;
   start 1; tree front.r; end; start 2; tree back.r; end; start 3; clean; end;" back.r)
matches(articulation.pix front.r)
matches(articulation.pix.1 front.r)
matches(articulation.pix.2 front.r)
matches(articulation.pix.3 back.r)
foreach(case IN ITEMS empty missing)
  if(case STREQUAL empty)
    set(stream "start 0; erase front.r; end;")
  else()
    set(stream "start 0; tree missing-object.r; end;")
  endif()
  set(input "${TEST_DIR}/${case}.view")
  set(output "${TEST_DIR}/${case}.pix")
  file(WRITE "${input}" "${POSE}; ${stream}\n")
  file(WRITE "${output}" "preserve existing output")
  run(FAIL INPUT_FILE "${input}" "${RT}" ${RENDER_ARGS} -M -o "${output}" "${DB}" front.r)
  file(READ "${output}" retained)
  if(NOT retained STREQUAL "preserve existing output")
    message(FATAL_ERROR "Invalid selection replaced existing output: ${case}")
  endif()
endforeach()
