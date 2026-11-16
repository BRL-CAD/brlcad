if(NOT DEFINED PIX_SUN OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "PIX_SUN and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/pix-sun-header-input.pix")
set(output_file "${TEST_DIR}/pix-sun-header-output.ras")
file(WRITE "${input_file}" "abcdef")

execute_process(
  COMMAND "${PIX_SUN}" -w 2 -n 1
  INPUT_FILE "${input_file}"
  OUTPUT_FILE "${output_file}"
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "pix-sun failed: ${error}")
endif()

file(SIZE "${output_file}" output_size)
if(NOT output_size EQUAL 802)
  message(FATAL_ERROR "pix-sun wrote a ${output_size}-byte file; expected 802")
endif()

file(READ "${output_file}" header_hex LIMIT 32 HEX)
set(expected_header "59a66a9500000002000000010000000800000002000000010000000100000300")
if(NOT header_hex STREQUAL expected_header)
  message(FATAL_ERROR "pix-sun wrote an invalid rasterfile header: ${header_hex}")
endif()
