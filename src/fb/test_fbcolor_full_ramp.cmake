if(NOT DEFINED FBCOLOR OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "FBCOLOR and TEST_DIR are required")
endif()

set(input_file "${TEST_DIR}/fbcolor-input.txt")
set(framebuffer_file "${TEST_DIR}/fbcolor-output.pix")
set(ramp_width 256)
file(REMOVE "${input_file}" "${framebuffer_file}")
file(WRITE "${input_file}" "q")

execute_process(
  COMMAND "${FBCOLOR}" -F "${framebuffer_file}" -w "${ramp_width}" -n 300
  INPUT_FILE "${input_file}"
  RESULT_VARIABLE fbcolor_result
  OUTPUT_QUIET
  ERROR_VARIABLE fbcolor_error
  TIMEOUT 30
)
if(NOT fbcolor_result EQUAL 0)
  message(FATAL_ERROR "fbcolor failed (${fbcolor_result}): ${fbcolor_error}")
endif()

# Pixel 255 is the last entry in the red ramp on the first scanline.
math(EXPR final_pixel_offset "(${ramp_width} - 1) * 3")
file(READ "${framebuffer_file}" final_red_pixel OFFSET "${final_pixel_offset}" LIMIT 3 HEX)
if(NOT final_red_pixel STREQUAL "ff0101")
  message(FATAL_ERROR "fbcolor ended its red ramp with ${final_red_pixel}, expected ff0101")
endif()

file(REMOVE "${input_file}" "${framebuffer_file}")
