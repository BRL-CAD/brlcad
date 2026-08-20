if(NOT DEFINED BWCROP OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWCROP and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/bwcrop-single-axis-input.bw")
set(output_file "${TEST_DIR}/bwcrop-single-axis-output.bw")

file(REMOVE "${input_file}" "${output_file}")
file(WRITE "${input_file}" "abcdefghijklmnopqr")

execute_process(
  COMMAND "${BWCROP}" "${input_file}" "${output_file}" 3 2 2 1 4 2 4 2 3 1 3
  RESULT_VARIABLE bwcrop_result
  ERROR_VARIABLE bwcrop_error
)
if(NOT bwcrop_result EQUAL 0)
  message(FATAL_ERROR "bwcrop buffered crop failed (${bwcrop_result}): ${bwcrop_error}")
endif()
file(READ "${output_file}" actual_output)
if(NOT actual_output STREQUAL "klno")
  message(FATAL_ERROR "bwcrop buffered crop produced '${actual_output}', expected 'klno'")
endif()

file(REMOVE "${output_file}")
execute_process(
  COMMAND "${BWCROP}" "${input_file}" "${output_file}" 3 1 1 0 2 2 2 2 0 0 0
  RESULT_VARIABLE bwcrop_result
  ERROR_VARIABLE bwcrop_error
)
if(NOT bwcrop_result EQUAL 0)
  message(FATAL_ERROR "bwcrop one-pixel crop failed (${bwcrop_result}): ${bwcrop_error}")
endif()
file(READ "${output_file}" actual_output)
if(NOT actual_output STREQUAL "e")
  message(FATAL_ERROR "bwcrop one-pixel crop produced '${actual_output}', expected 'e'")
endif()

file(REMOVE "${input_file}" "${output_file}")
