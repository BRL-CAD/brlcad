# Verify the default PIX mode grays matching pixels and highlights only the
# channels that differ within a pixel.

if(NOT DEFINED PIXDIFF OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "PIXDIFF and TEST_DIR are required")
endif()

set(input_file_1 "${TEST_DIR}/pixdiff-pix-input-1.pix")
set(input_file_2 "${TEST_DIR}/pixdiff-pix-input-2.pix")
set(output_file "${TEST_DIR}/pixdiff-pix-output.pix")

file(REMOVE "${input_file_1}" "${input_file_2}" "${output_file}")

# RGB is a matching pixel with distinct channels.  ACE/ADH has one
# matching channel, one off by 1, and one off by many.
file(WRITE "${input_file_1}" "RGBACE")
file(WRITE "${input_file_2}" "RGBADH")

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
if(NOT output_hex STREQUAL "12121200c0ff")
  message(FATAL_ERROR "pixdiff output was ${output_hex}, expected 12121200c0ff")
endif()

if(NOT pixdiff_statistics MATCHES "pixdiff bytes: +4 matching, +1 off by 1, +1 off by many")
  message(FATAL_ERROR "unexpected pixdiff statistics: ${pixdiff_statistics}")
endif()

# A partial RGB triplet must not be accepted, even when both files have
# the same number of bytes.
file(WRITE "${input_file_1}" "RGBX")
file(WRITE "${input_file_2}" "RGBX")
execute_process(
  COMMAND "${PIXDIFF}" "${input_file_1}" "${input_file_2}"
  OUTPUT_FILE "${output_file}"
  ERROR_VARIABLE pixdiff_error
  RESULT_VARIABLE pixdiff_result
)
if(pixdiff_result EQUAL 0)
  message(FATAL_ERROR "pixdiff accepted an incomplete RGB pixel")
endif()

file(REMOVE "${input_file_1}" "${input_file_2}" "${output_file}")
