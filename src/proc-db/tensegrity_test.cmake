if(NOT DEFINED TENSEGRITY OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "TENSEGRITY and TEST_DIR are required")
endif()

set(output_file "${TEST_DIR}/tensegrity-test.g")
file(REMOVE "${output_file}")

execute_process(
  COMMAND "${TENSEGRITY}" "${output_file}" --stages 2 --radius 180 --height 160 --twist 52
  RESULT_VARIABLE tensegrity_result
  OUTPUT_VARIABLE tensegrity_output
  ERROR_VARIABLE tensegrity_error
)
if(NOT tensegrity_result EQUAL 0)
  message(FATAL_ERROR "tensegrity generation failed (${tensegrity_result}): ${tensegrity_output}${tensegrity_error}")
endif()

if(NOT EXISTS "${output_file}")
  message(FATAL_ERROR "tensegrity did not create ${output_file}")
endif()

file(SIZE "${output_file}" output_size)
if(output_size LESS 1024)
  message(FATAL_ERROR "tensegrity database is unexpectedly small (${output_size} bytes)")
endif()

file(REMOVE "${output_file}")
