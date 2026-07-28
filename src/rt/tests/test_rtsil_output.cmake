if(NOT DEFINED RTSIL OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RTSIL, DB, and TEST_DIR are required")
endif()

set(image_side 32)
math(EXPR expected_size "${image_side} * ${image_side}")
set(stdout_file "${TEST_DIR}/rtsil-stdout.bw")
set(output_file "${TEST_DIR}/rtsil-option.bw")

file(REMOVE "${stdout_file}" "${output_file}")

execute_process(
  COMMAND "${RTSIL}" -P 1 -s "${image_side}" "${DB}" all.g
  OUTPUT_FILE "${stdout_file}"
  ERROR_VARIABLE stdout_log
  RESULT_VARIABLE stdout_result
)
if(NOT stdout_result EQUAL 0)
  message(FATAL_ERROR "rtsil redirected-output render failed:\n${stdout_log}")
endif()

foreach(render_pass RANGE 1 2)
  execute_process(
    COMMAND "${RTSIL}" -P 1 -s "${image_side}" -o "${output_file}" "${DB}" all.g
    OUTPUT_VARIABLE option_stdout
    ERROR_VARIABLE output_log
    RESULT_VARIABLE output_result
  )
  if(NOT output_result EQUAL 0)
    message(FATAL_ERROR "rtsil -o render ${render_pass} failed:\n${output_log}")
  endif()
  if(NOT option_stdout STREQUAL "")
    message(FATAL_ERROR "rtsil -o unexpectedly wrote image data to stdout")
  endif()
endforeach()

foreach(image_file IN ITEMS "${stdout_file}" "${output_file}")
  if(NOT EXISTS "${image_file}")
    message(FATAL_ERROR "rtsil did not create ${image_file}")
  endif()
  file(SIZE "${image_file}" image_size)
  if(NOT image_size EQUAL expected_size)
    message(
      FATAL_ERROR
      "Expected ${expected_size} bytes in ${image_file}, found ${image_size}"
    )
  endif()
endforeach()

file(SHA256 "${stdout_file}" stdout_hash)
file(SHA256 "${output_file}" output_hash)
if(NOT stdout_hash STREQUAL output_hash)
  message(FATAL_ERROR "rtsil -o output differs from redirected stdout output")
endif()

file(READ "${output_file}" output_hex HEX)
if(NOT output_hex MATCHES "01" OR NOT output_hex MATCHES "ff")
  message(FATAL_ERROR "rtsil output does not contain both hit and miss pixels")
endif()
string(REGEX REPLACE "(01|ff)" "" unexpected_pixels "${output_hex}")
if(NOT unexpected_pixels STREQUAL "")
  message(FATAL_ERROR "rtsil output contains values other than 1 and 255")
endif()

file(REMOVE "${stdout_file}" "${output_file}")
