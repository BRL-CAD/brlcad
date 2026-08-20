if(NOT DEFINED BWSTAT OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWSTAT and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/bwstat-input.bw")
set(empty_file "${TEST_DIR}/bwstat-empty.bw")
file(REMOVE "${input_file}" "${empty_file}")

string(ASCII 1 1 255 input_data)
file(WRITE "${input_file}" "${input_data}")
file(WRITE "${empty_file}" "")

execute_process(
  COMMAND "${BWSTAT}" "${input_file}"
  RESULT_VARIABLE bwstat_result
  OUTPUT_VARIABLE bwstat_output
  ERROR_VARIABLE bwstat_error
)
if(NOT bwstat_result EQUAL 0)
  message(FATAL_ERROR "bwstat failed (${bwstat_result}): ${bwstat_error}")
endif()
if(NOT bwstat_output MATCHES "Median  +1")
  message(FATAL_ERROR "bwstat reported the wrong median:\n${bwstat_output}")
endif()

execute_process(
  COMMAND "${BWSTAT}" "${empty_file}"
  RESULT_VARIABLE bwstat_result
  OUTPUT_VARIABLE bwstat_output
  ERROR_VARIABLE bwstat_error
)
if(bwstat_result EQUAL 0)
  message(FATAL_ERROR "bwstat accepted empty input:\n${bwstat_output}")
endif()
if(NOT bwstat_error MATCHES "input contains no pixels")
  message(FATAL_ERROR "bwstat did not explain the empty input error:\n${bwstat_error}")
endif()

file(REMOVE "${input_file}" "${empty_file}")
