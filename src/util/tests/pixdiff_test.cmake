# Verify that pixdiff handles arbitrary-length byte streams without format
# switches and reports classifications in bytes.

if(NOT PIXDIFF OR NOT TEST_DIR)
  message(FATAL_ERROR "PIXDIFF and TEST_DIR are required")
endif()

set(input1 "${TEST_DIR}/pixdiff_test_1.bin")
set(input2 "${TEST_DIR}/pixdiff_test_2.bin")
set(output "${TEST_DIR}/pixdiff_test_output.bin")

# Four bytes deliberately cannot be interpreted as complete RGB pixels.
file(WRITE "${input1}" "ACEG")
file(WRITE "${input2}" "ADGG")

execute_process(
  COMMAND "${PIXDIFF}" "${input1}" "${input2}"
  OUTPUT_FILE "${output}"
  ERROR_VARIABLE statistics
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "pixdiff failed with status ${result}: ${statistics}")
endif()

file(READ "${output}" output_hex HEX)
if(NOT output_hex STREQUAL "20c0ff23")
  message(FATAL_ERROR "pixdiff output was ${output_hex}, expected 20c0ff23")
endif()

if(NOT statistics MATCHES "pixdiff bytes: +2 matching, +1 off by 1, +1 off by many")
  message(FATAL_ERROR "unexpected pixdiff statistics: ${statistics}")
endif()

file(REMOVE "${input1}" "${input2}" "${output}")
