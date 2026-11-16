if(NOT DEFINED PIX_PPM OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "PIX_PPM and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/pix-ppm-grayscale-input.bw")
set(output_file "${TEST_DIR}/pix-ppm-grayscale-output.pgm")
string(ASCII 127 pixel)
file(WRITE "${input_file}" "${pixel}")

execute_process(
  COMMAND "${PIX_PPM}" "-#" 1 -s 1 "${input_file}"
  OUTPUT_FILE "${output_file}"
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "pix-ppm failed: ${error}")
endif()

file(READ "${output_file}" output_hex HEX)
if(NOT output_hex STREQUAL "50350a3120310a3235350a7f")
  message(FATAL_ERROR "pix-ppm did not write binary PGM data: ${output_hex}")
endif()

execute_process(
  COMMAND "${PIX_PPM}" "-#" 3 -s 1 "${input_file}"
  OUTPUT_QUIET
  ERROR_QUIET
  RESULT_VARIABLE short_result
)
if(short_result EQUAL 0)
  message(FATAL_ERROR "pix-ppm accepted a partial input pixel")
endif()
