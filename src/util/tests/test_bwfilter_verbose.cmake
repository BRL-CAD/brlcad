if(NOT DEFINED BWFILTER OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWFILTER and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/bwfilter-verbose-input.bw")
set(output_file "${TEST_DIR}/bwfilter-verbose-output.bw")
file(WRITE "${input_file}" "ddddddddd")

execute_process(
  COMMAND "${BWFILTER}" -v -s 3 "${input_file}"
  OUTPUT_FILE "${output_file}"
  RESULT_VARIABLE result
  ERROR_VARIABLE diagnostics
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bwfilter failed: ${diagnostics}")
endif()

string(FIND "${diagnostics}" "kern[8]" last_kernel)
string(FIND "${diagnostics}" "kern[9]" out_of_bounds_kernel)
if(last_kernel EQUAL -1 OR NOT out_of_bounds_kernel EQUAL -1)
  message(FATAL_ERROR "bwfilter reported values beyond its 9-element kernel: ${diagnostics}")
endif()
