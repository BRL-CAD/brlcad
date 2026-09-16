if(NOT DEFINED RT OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RT, DB, and TEST_DIR are required")
endif()

set(image_side 20)
math(EXPR expected_size "${image_side} * ${image_side} * 3")
set(output_file "${TEST_DIR}/rt-disk-framebuffer.pix")

file(REMOVE "${output_file}")

execute_process(
  COMMAND "${RT}" -P 1 -s "${image_side}" -F "${output_file}" "${DB}" all.g
  OUTPUT_VARIABLE render_stdout
  ERROR_VARIABLE render_stderr
  RESULT_VARIABLE render_result
)
if(NOT render_result EQUAL 0)
  message(
    FATAL_ERROR
    "rt disk framebuffer render failed:\n${render_stdout}${render_stderr}"
  )
endif()

if(NOT EXISTS "${output_file}")
  message(FATAL_ERROR "rt did not create ${output_file}")
endif()

file(SIZE "${output_file}" output_size)
if(NOT output_size EQUAL expected_size)
  message(
    FATAL_ERROR
    "Expected ${expected_size} bytes in ${output_file}, found ${output_size}"
  )
endif()

file(REMOVE "${output_file}")
