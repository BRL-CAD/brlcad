if(NOT DEFINED RT OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RT, DB, and TEST_DIR are required")
endif()

# Widths up to 96 use view_pixel's single-pixel I/O path.
set(image_side 32)
set(output_file "${TEST_DIR}/rt-unbuffered-background.pix")
set(background "255/0/0")

file(REMOVE "${output_file}")
execute_process(
  COMMAND
    "${RT}" -P 1 -s "${image_side}" -C "${background}"
    -o "${output_file}" "${DB}" all.g
  OUTPUT_VARIABLE render_stdout
  ERROR_VARIABLE render_log
  RESULT_VARIABLE render_result
)
if(NOT render_result EQUAL 0)
  message(FATAL_ERROR "rt background render failed:\n${render_log}")
endif()
if(NOT render_stdout STREQUAL "")
  message(FATAL_ERROR "rt unexpectedly wrote image data to stdout")
endif()

file(READ "${output_file}" first_pixel LIMIT 3 HEX)
if(NOT first_pixel STREQUAL "ff0000")
  message(
    FATAL_ERROR
    "Expected first background pixel ff0000, found ${first_pixel}"
  )
endif()

file(REMOVE "${output_file}")
