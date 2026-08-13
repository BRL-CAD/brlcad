# Exercise pixcmp's pixel tolerance and exit-status contract with small,
# deliberately readable byte streams.

if(NOT PIXCMP OR NOT TEST_DIR)
  message(FATAL_ERROR "PIXCMP and TEST_DIR are required")
endif()

function(check_pixcmp name input1 input2 expected_status expected_summary)
  set(file1 "${TEST_DIR}/pixcmp_${name}_1.pix")
  set(file2 "${TEST_DIR}/pixcmp_${name}_2.pix")

  file(WRITE "${file1}" "${input1}")
  file(WRITE "${file2}" "${input2}")

  execute_process(
    COMMAND "${PIXCMP}" ${ARGN} "${file1}" "${file2}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
  )

  file(REMOVE "${file1}" "${file2}")

  if(NOT "${status}" STREQUAL "${expected_status}")
    message(
      FATAL_ERROR
      "${name}: pixcmp returned ${status}, expected ${expected_status}\nstdout: ${output}\nstderr: ${errors}"
    )
  endif()

  string(REGEX REPLACE "[ \t\r\n]+" " " normalized_output "${output}")
  string(STRIP "${normalized_output}" normalized_output)
  string(FIND "${normalized_output}" "${expected_summary}" summary_position)
  if(summary_position EQUAL -1)
    message(
      FATAL_ERROR
      "${name}: expected summary '${expected_summary}'\nactual stdout: ${output}\nstderr: ${errors}"
    )
  endif()
endfunction()

set(exact_pixel "pixcmp pixels: 1 matching, 0 off by 1, 0 off by many")
set(tolerated_pixel "pixcmp pixels: 0 matching, 1 off by 1, 0 off by many")
set(rejected_pixel "pixcmp pixels: 0 matching, 0 off by 1, 1 off by many")

check_pixcmp(exact "ABC" "ABC" 0 "${exact_pixel}")
check_pixcmp(one_channel_within_tolerance "ABC" "BBC" 1 "${tolerated_pixel}")
check_pixcmp(two_channels_within_tolerance "ABC" "BCC" 1 "${tolerated_pixel}")
check_pixcmp(three_channels_within_tolerance "ABC" "@CB" 1 "${tolerated_pixel}")

# Keep the tolerated channel first in two cases to catch short-circuiting that
# would fail to inspect a later channel outside the tolerance.
check_pixcmp(red_outside_tolerance "ABC" "CCD" 2 "${rejected_pixel}")
check_pixcmp(green_outside_tolerance "ABC" "BDD" 2 "${rejected_pixel}")
check_pixcmp(blue_outside_tolerance "ABC" "BCE" 2 "${rejected_pixel}")

check_pixcmp(
  missing_pixel
  "ABC"
  "ABCDEF"
  2
  "pixcmp pixels: 1 matching, 0 off by 1, 0 off by many, 1 missing"
)

check_pixcmp(
  byte_within_tolerance
  "A"
  "B"
  1
  "pixcmp bytes: 0 matching, 1 off by 1, 0 off by many"
  -b
)
check_pixcmp(
  byte_outside_tolerance
  "A"
  "C"
  2
  "pixcmp bytes: 0 matching, 0 off by 1, 1 off by many"
  -b
)
