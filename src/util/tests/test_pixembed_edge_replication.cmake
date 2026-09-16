if(NOT DEFINED PIXEMBED OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "PIXEMBED and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/pixembed-edge-replication-input.pix")
set(output_file "${TEST_DIR}/pixembed-edge-replication-output.pix")

file(REMOVE "${input_file}" "${output_file}")
file(WRITE "${input_file}" "aaabbbcccdddeeefffggghhhiiijjjkkklllmmmnnnoooppp")

execute_process(
  COMMAND "${PIXEMBED}" -w 4 -n 4 -W 7 -N 7 -b 1 "${input_file}"
  OUTPUT_FILE "${output_file}"
  RESULT_VARIABLE pixembed_result
  ERROR_VARIABLE pixembed_error
)
if(NOT pixembed_result EQUAL 0)
  message(FATAL_ERROR "pixembed edge replication failed (${pixembed_result}): ${pixembed_error}")
endif()

string(REPEAT "fffffffffgggggggggggg" 3 expected_bottom)
string(REPEAT "jjjjjjjjjkkkkkkkkkkkk" 4 expected_top)
string(HEX "${expected_bottom}${expected_top}" expected_hex)
file(READ "${output_file}" actual_hex HEX)
if(NOT actual_hex STREQUAL expected_hex)
  message(FATAL_ERROR "pixembed output was ${actual_hex}, expected ${expected_hex}")
endif()

file(REMOVE "${input_file}" "${output_file}")
