if(NOT DEFINED BWCROP OR NOT DEFINED PIXEMBED OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWCROP, PIXEMBED, and TEST_DIR are required")
endif()

set(bw_input "${TEST_DIR}/bwcrop_test_input.bw")
set(bw_output "${TEST_DIR}/bwcrop_test_output.bw")
file(WRITE "${bw_input}" "abcdefghijklmnopqr")

execute_process(
  COMMAND "${BWCROP}" "${bw_input}" "${bw_output}" 3 2 2 1 4 2 4 2 3 1 3
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bwcrop buffered crop failed (${result}): ${error}")
endif()
file(READ "${bw_output}" actual)
if(NOT actual STREQUAL "klno")
  message(FATAL_ERROR "bwcrop buffered crop produced '${actual}', expected 'klno'")
endif()

execute_process(
  COMMAND "${BWCROP}" "${bw_input}" "${bw_output}" 3 1 1 0 2 2 2 2 0 0 0
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "bwcrop one-pixel crop failed (${result}): ${error}")
endif()
file(READ "${bw_output}" actual)
if(NOT actual STREQUAL "e")
  message(FATAL_ERROR "bwcrop one-pixel crop produced '${actual}', expected 'e'")
endif()

set(pix_input "${TEST_DIR}/pixembed_test_input.pix")
set(pix_output "${TEST_DIR}/pixembed_test_output.pix")
file(WRITE "${pix_input}" "aaabbbcccdddeeefffggghhhiiijjjkkklllmmmnnnoooppp")

execute_process(
  COMMAND "${PIXEMBED}" -w 4 -n 4 -W 7 -N 7 -b 1 "${pix_input}"
  OUTPUT_FILE "${pix_output}"
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "pixembed inset embedding failed (${result}): ${error}")
endif()

string(REPEAT "fffffffffgggggggggggg" 3 expected_bottom)
string(REPEAT "jjjjjjjjjkkkkkkkkkkkk" 4 expected_top)
file(READ "${pix_output}" actual)
if(NOT actual STREQUAL "${expected_bottom}${expected_top}")
  message(FATAL_ERROR "pixembed inset or odd-margin output is incorrect")
endif()
