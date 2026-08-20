if(NOT DEFINED RT OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RT, DB, and TEST_DIR are required")
endif()

function(assert_rt_view case_name expected_eye expected_orientation)
  set(output_file "${TEST_DIR}/rt-command-view-${case_name}.pix")

  file(REMOVE "${output_file}")
  execute_process(
    COMMAND
      "${RT}" -P 1 -B -s 1 -v ${ARGN}
      -o "${output_file}" "${DB}" all.g
    OUTPUT_VARIABLE render_stdout
    ERROR_VARIABLE render_stderr
    RESULT_VARIABLE render_result
  )
  file(REMOVE "${output_file}")

  set(render_log "${render_stdout}${render_stderr}")
  if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "${case_name} render failed:\n${render_log}")
  endif()

  string(FIND "${render_log}" "${expected_eye}" eye_offset)
  if(eye_offset EQUAL -1)
    message(
      FATAL_ERROR
      "${case_name} did not preserve its eye point; expected '${expected_eye}':\n${render_log}"
    )
  endif()

  if(NOT expected_orientation STREQUAL "")
    string(FIND "${render_log}" "${expected_orientation}" orientation_offset)
    if(orientation_offset EQUAL -1)
      message(
        FATAL_ERROR
        "${case_name} did not preserve its orientation; expected '${expected_orientation}':\n${render_log}"
      )
    endif()
  endif()
endfunction()

assert_rt_view(
  eye_only
  "Eye_pos: 63.7999, 32.7177, 33.6666"
  ""
  -c "viewsize 157.203"
  -c "eye_pt 63.7999 32.7177 33.6666"
)

assert_rt_view(
  viewrot
  "Eye_pos: 10, 20, 30"
  "Orientation: 0, 0, 0, 1"
  -c "viewsize 157.203"
  -c "eye_pt 10 20 30"
  -c "viewrot 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"
)

assert_rt_view(
  lookat
  "Eye_pos: 100, 50, 25"
  "View: 26.5651 azimuth, 12.6044 elevation"
  -c "viewsize 157.203"
  -c "eye_pt 100 50 25"
  -c "lookat_pt 0 0 0 1"
)
