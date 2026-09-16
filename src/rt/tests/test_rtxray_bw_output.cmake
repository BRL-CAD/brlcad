if(NOT DEFINED RTXRAY OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RTXRAY, DB, and TEST_DIR are required")
endif()

set(image_size 16)
set(output_file "${TEST_DIR}/rtxray-output.bw")
set(framebuffer_file "${TEST_DIR}/rtxray-framebuffer.pix")
file(REMOVE "${output_file}" "${framebuffer_file}")

execute_process(
  COMMAND "${RTXRAY}" -P 1 -s "${image_size}" -o "${output_file}" -F "${framebuffer_file}" "${DB}" all.g
  OUTPUT_VARIABLE render_output
  ERROR_VARIABLE render_log
  RESULT_VARIABLE render_result
  TIMEOUT 30
)

if(NOT render_result EQUAL 0)
  message(FATAL_ERROR "rtxray render failed:\n${render_log}")
endif()
if(NOT render_output STREQUAL "")
  message(FATAL_ERROR "rtxray -o unexpectedly wrote image data to stdout")
endif()
if(NOT EXISTS "${output_file}")
  message(FATAL_ERROR "rtxray did not create ${output_file}")
endif()

math(EXPR expected_size "${image_size} * ${image_size}")
file(SIZE "${output_file}" actual_size)
if(NOT actual_size EQUAL expected_size)
  message(FATAL_ERROR "Expected ${expected_size} bytes of BW data, found ${actual_size}")
endif()

math(EXPR expected_framebuffer_size "${expected_size} * 3")
file(SIZE "${framebuffer_file}" actual_framebuffer_size)
if(NOT actual_framebuffer_size EQUAL expected_framebuffer_size)
  message(FATAL_ERROR "Expected ${expected_framebuffer_size} framebuffer bytes, found ${actual_framebuffer_size}")
endif()

file(REMOVE "${output_file}" "${framebuffer_file}")
