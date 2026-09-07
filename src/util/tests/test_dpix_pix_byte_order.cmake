if(NOT DEFINED DPIX_PIX OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "DPIX_PIX and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/dpix-pix-byte-order-input.dpix")
set(output_file "${TEST_DIR}/dpix-pix-byte-order-output.pix")
file(REMOVE "${input_file}" "${output_file}")

# Portable big-endian encodings of 0.1, 0.2, and 0.4.  These values avoid
# zero bytes so CMake can construct the binary fixture as a string.
string(
  ASCII
  63 185 153 153 153 153 153 154
  63 201 153 153 153 153 153 154
  63 217 153 153 153 153 153 154
  input_data
)
file(WRITE "${input_file}" "${input_data}")

execute_process(
  COMMAND "${DPIX_PIX}" "${input_file}"
  RESULT_VARIABLE dpix_pix_result
  OUTPUT_FILE "${output_file}"
  ERROR_VARIABLE dpix_pix_error
)
if(NOT dpix_pix_result EQUAL 0)
  message(FATAL_ERROR "dpix-pix failed (${dpix_pix_result}): ${dpix_pix_error}")
endif()

file(READ "${output_file}" output_hex HEX)
if(NOT output_hex STREQUAL "0055ff")
  message(FATAL_ERROR "dpix-pix output was ${output_hex}, expected 0055ff")
endif()

file(REMOVE "${input_file}" "${output_file}")
