#                   I C V _ C O N V E R S I O N . C M A K E
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
if(NOT ICV OR NOT FIXTURE OR NOT TEST_DIR)
  message(FATAL_ERROR "ICV, FIXTURE, and TEST_DIR are required")
endif()
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")
set(LOG "${TEST_DIR}/commands.log")
function(run name should_fail)
  cmake_parse_arguments(PARSE_ARGV 2 options "" "INPUT_FILE;OUTPUT_FILE" "")
  set(streams)
  foreach(stream IN ITEMS INPUT_FILE OUTPUT_FILE)
    if(DEFINED options_${stream})
      list(APPEND streams ${stream} "${options_${stream}}")
    endif()
  endforeach()
  set(output "")
  if(NOT DEFINED options_OUTPUT_FILE)
    list(APPEND streams OUTPUT_VARIABLE output)
  endif()
  execute_process(COMMAND ${options_UNPARSED_ARGUMENTS} ${streams}
    RESULT_VARIABLE result ERROR_VARIABLE errors TIMEOUT 30)
  file(APPEND "${LOG}" "${name}: ${options_UNPARSED_ARGUMENTS}\n${output}${errors}\n")
  if(NOT "${result}" MATCHES "^[0-9]+$" OR
      (should_fail AND result EQUAL 0) OR (NOT should_fail AND NOT result EQUAL 0))
    message(FATAL_ERROR "Unexpected conversion result ${result} for ${name}: ${output}${errors}")
  endif()
endfunction()
set(source "${TEST_DIR}/source.exr")
set(copy "${TEST_DIR}/copy.exr")
run(source FALSE OUTPUT_FILE "${source}" "${FIXTURE}" --emit)
run(files FALSE "${ICV}" "${source}" "${copy}")
run(check_files FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
run(input_option FALSE OUTPUT_FILE "${copy}" "${ICV}" -i "${source}" --output-format exr)
run(check_input_option FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
file(REMOVE "${copy}")
run(output_option FALSE INPUT_FILE "${source}" "${ICV}" --input-format exr -o "${copy}")
run(check_output_option FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
run(positional_input FALSE OUTPUT_FILE "${copy}" "${ICV}" --output-format exr "${source}")
run(check_positional_input FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
run(positional_output FALSE "${ICV}" -i "${source}" "${copy}")
run(check_positional_output FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
file(REMOVE "${copy}")
run(explicit_output FALSE "${ICV}" -o "${copy}" "${source}")
run(check_explicit_output FALSE INPUT_FILE "${copy}" "${FIXTURE}" --check-stream)
execute_process(COMMAND "${FIXTURE}" --emit
  COMMAND "${ICV}" --input-format exr --output-format exr
  COMMAND "${FIXTURE}" --check-stream RESULTS_VARIABLE results
  ERROR_VARIABLE errors TIMEOUT 30)
file(APPEND "${LOG}" "pipe: ${results}\n${errors}\n")
foreach(result IN LISTS results)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "CLI EXR pipe failed (${results}): ${errors}")
  endif()
endforeach()

run(png FALSE "${ICV}" "${source}" "${TEST_DIR}/preview.png")
run(png_copy FALSE "${ICV}" "${TEST_DIR}/preview.png" "${TEST_DIR}/preview-copy.png")
run(png_pixels FALSE "${ICV}" diff "${TEST_DIR}/preview.png" "${TEST_DIR}/preview-copy.png")

set(sentinel "preserve existing output")
file(WRITE "${TEST_DIR}/truncated.exr" "not an EXR image")
foreach(case IN ITEMS missing truncated parsing extra_arguments)
  file(WRITE "${copy}" "${sentinel}")
  if(case STREQUAL missing OR case STREQUAL truncated)
    run("${case}" TRUE "${ICV}" "${TEST_DIR}/${case}.exr" "${copy}")
  elseif(case STREQUAL parsing)
    run(parsing TRUE "${ICV}" --not-an-option "${source}" "${copy}")
  else()
    run(extra_arguments TRUE "${ICV}" "${source}" "${copy}" unexpected)
  endif()
  file(READ "${copy}" retained)
  if(NOT retained STREQUAL sentinel)
    message(FATAL_ERROR "Failed conversion replaced existing output: ${case}")
  endif()
endforeach()
file(WRITE "${copy}" "${sentinel}")
run(existing_output TRUE "${ICV}" -i "${source}" -o "${copy}")
file(READ "${copy}" retained)
if(NOT retained STREQUAL sentinel)
  message(FATAL_ERROR "Refused explicit output replaced an existing file")
endif()
run(write_failure TRUE "${ICV}" "${source}" "${TEST_DIR}/missing-directory/output.exr")
file(SHA256 "${source}" before)
run(same_path TRUE "${ICV}" "${source}" "${source}")
file(SHA256 "${source}" after)
if(NOT before STREQUAL after)
  message(FATAL_ERROR "Identical input/output conversion changed the source")
endif()
