if(NOT DEFINED BWFILTER OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWFILTER and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/bwfilter-divisor-input.bw")
set(output_file "${TEST_DIR}/bwfilter-divisor-output.bw")
file(WRITE "${input_file}" "ddddddddd")

execute_process(
  COMMAND "${BWFILTER}" -s 3 -d 84 "${input_file}"
  OUTPUT_FILE "${output_file}"
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bwfilter failed: ${error}")
endif()

file(READ "${output_file}" actual)
if(NOT actual STREQUAL "222222222")
  string(HEX "${actual}" actual_hex)
  message(FATAL_ERROR "bwfilter ignored its divisor override: ${actual_hex}")
endif()
