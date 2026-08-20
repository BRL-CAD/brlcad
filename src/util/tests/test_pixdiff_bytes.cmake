# Verify that pixdiff handles arbitrary-length byte streams without format
# switches and reports classifications in bytes.

if(NOT DEFINED PIXDIFF OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "PIXDIFF and TEST_DIR are required")
endif()

set(input_file_1 "${TEST_DIR}/pixdiff-bytes-input-1.bin")
set(input_file_2 "${TEST_DIR}/pixdiff-bytes-input-2.bin")
set(output_file "${TEST_DIR}/pixdiff-bytes-output.bin")

file(REMOVE "${input_file_1}" "${input_file_2}" "${output_file}")

# Four bytes deliberately cannot be interpreted as complete RGB pixels.
file(WRITE "${input_file_1}" "ACEG")
file(WRITE "${input_file_2}" "ADGG")

execute_process(
  COMMAND "${PIXDIFF}" "${input_file_1}" "${input_file_2}"
  OUTPUT_FILE "${output_file}"
  ERROR_VARIABLE pixdiff_statistics
  RESULT_VARIABLE pixdiff_result
)

if(NOT pixdiff_result EQUAL 0)
  message(FATAL_ERROR "pixdiff failed with status ${pixdiff_result}: ${pixdiff_statistics}")
endif()

file(READ "${output_file}" output_hex HEX)
if(NOT output_hex STREQUAL "20c0ff23")
  message(FATAL_ERROR "pixdiff output was ${output_hex}, expected 20c0ff23")
endif()

if(NOT pixdiff_statistics MATCHES "pixdiff bytes: +2 matching, +1 off by 1, +1 off by many")
  message(FATAL_ERROR "unexpected pixdiff statistics: ${pixdiff_statistics}")
endif()

file(REMOVE "${input_file_1}" "${input_file_2}" "${output_file}")
