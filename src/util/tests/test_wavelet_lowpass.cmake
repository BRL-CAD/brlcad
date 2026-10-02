if(NOT DEFINED WAVELET OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "WAVELET and TEST_DIR are required")
endif()

set(one_dimensional_input "${TEST_DIR}/wavelet-lowpass-1d-input.bin")
set(one_dimensional_output "${TEST_DIR}/wavelet-lowpass-1d-output.bin")
set(two_dimensional_input "${TEST_DIR}/wavelet-lowpass-2d-input.bin")
set(two_dimensional_output "${TEST_DIR}/wavelet-lowpass-2d-output.bin")

file(REMOVE
  "${one_dimensional_input}"
  "${one_dimensional_output}"
  "${two_dimensional_input}"
  "${two_dimensional_output}"
)

string(ASCII 12 16 32 36 one_dimensional_data)
file(WRITE "${one_dimensional_input}" "${one_dimensional_data}")

execute_process(
  COMMAND "${WAVELET}" -p 2 -1 -t c "-#" 1 -w 4 -n 1
  INPUT_FILE "${one_dimensional_input}"
  OUTPUT_FILE "${one_dimensional_output}"
  RESULT_VARIABLE wavelet_result
  ERROR_VARIABLE wavelet_error
)
if(NOT wavelet_result EQUAL 0)
  message(FATAL_ERROR "wavelet 1D low-pass reconstruction failed (${wavelet_result}): ${wavelet_error}")
endif()

file(READ "${one_dimensional_output}" one_dimensional_output_hex HEX)
if(NOT one_dimensional_output_hex STREQUAL "0e0e2222")
  message(FATAL_ERROR "wavelet 1D low-pass output was ${one_dimensional_output_hex}, expected 0e0e2222")
endif()

string(ASCII 32 34 36 38 40 42 44 46 48 50 52 54 56 58 60 62 two_dimensional_data)
file(WRITE "${two_dimensional_input}" "${two_dimensional_data}")

execute_process(
  COMMAND "${WAVELET}" -p 2 -2 -t c "-#" 1 -s 4
  INPUT_FILE "${two_dimensional_input}"
  OUTPUT_FILE "${two_dimensional_output}"
  RESULT_VARIABLE wavelet_result
  ERROR_VARIABLE wavelet_error
)
if(NOT wavelet_result EQUAL 0)
  message(FATAL_ERROR "wavelet 2D low-pass reconstruction failed (${wavelet_result}): ${wavelet_error}")
endif()

file(READ "${two_dimensional_output}" two_dimensional_output_hex HEX)
if(NOT two_dimensional_output_hex STREQUAL "25252929252529293535393935353939")
  message(FATAL_ERROR "wavelet 2D low-pass output was ${two_dimensional_output_hex}, expected 25252929252529293535393935353939")
endif()

file(REMOVE
  "${one_dimensional_input}"
  "${one_dimensional_output}"
  "${two_dimensional_input}"
  "${two_dimensional_output}"
)
