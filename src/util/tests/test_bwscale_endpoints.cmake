if(NOT DEFINED BWSCALE OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "BWSCALE and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/bwscale-endpoints-input.bw")
string(ASCII 1 2 3 255 input_data)
file(REMOVE "${input_file}")
file(WRITE "${input_file}" "${input_data}")

set(corner_offsets 0 3 12 15)
set(expected_corners 01 02 03 ff)
foreach(method IN ITEMS bilinear nearest)
  set(output_file "${TEST_DIR}/bwscale-endpoints-${method}.bw")
  set(method_option)
  if(method STREQUAL "nearest")
    set(method_option -r)
  endif()

  file(REMOVE "${output_file}")
  execute_process(
    COMMAND "${BWSCALE}" ${method_option} -w 2 -n 2 -W 4 -N 4 "${input_file}"
    OUTPUT_FILE "${output_file}"
    RESULT_VARIABLE bwscale_result
    ERROR_VARIABLE bwscale_error
  )
  if(NOT bwscale_result EQUAL 0)
    message(FATAL_ERROR "bwscale ${method} mode failed (${bwscale_result}): ${bwscale_error}")
  endif()

  file(SIZE "${output_file}" output_size)
  if(NOT output_size EQUAL 16)
    message(FATAL_ERROR "bwscale ${method} mode wrote ${output_size} bytes, expected 16")
  endif()

  foreach(index RANGE 0 3)
    list(GET corner_offsets ${index} offset)
    list(GET expected_corners ${index} expected)
    file(READ "${output_file}" actual OFFSET ${offset} LIMIT 1 HEX)
    if(NOT actual STREQUAL expected)
      message(FATAL_ERROR "bwscale ${method} corner at offset ${offset} was ${actual}, expected ${expected}")
    endif()
  endforeach()
  if(method STREQUAL "nearest")
    file(READ "${output_file}" actual HEX)
    set(expected "01010202010102020303ffff0303ffff")
    if(NOT actual STREQUAL expected)
      message(FATAL_ERROR "bwscale nearest-neighbor output was ${actual}, expected ${expected}")
    endif()
  endif()
  file(REMOVE "${output_file}")
endforeach()

file(REMOVE "${input_file}")
