if(NOT DEFINED MGED_EXECUTABLE)
  message(FATAL_ERROR "MGED_EXECUTABLE is not defined")
endif()

if(NOT DEFINED DB_FILE)
  message(FATAL_ERROR "DB_FILE is not defined")
endif()

if(NOT DEFINED INPUT_FILE)
  message(FATAL_ERROR "INPUT_FILE is not defined")
endif()

execute_process(
  COMMAND "${MGED_EXECUTABLE}" -c "${DB_FILE}"
  INPUT_FILE "${INPUT_FILE}"
  RESULT_VARIABLE mged_result
  OUTPUT_VARIABLE mged_output
  ERROR_VARIABLE mged_error
  TIMEOUT 30
)

if(NOT mged_result EQUAL 0)
  message(FATAL_ERROR "mged exited with ${mged_result}\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
endif()

set(mged_combined_output "${mged_output}\n${mged_error}")

set(expected_common "1 {draw all.g/tor.r rest} tor.r 1 16")
string(FIND "${mged_combined_output}" "${expected_common}" common_pos)
if(common_pos EQUAL -1)
  message(FATAL_ERROR "missing common completion result [${expected_common}]\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
endif()

set(expected_cycle "1 {draw all.g/tor.r rest} tor.r 1 16")
string(FIND "${mged_combined_output}" "${expected_cycle}" cycle_pos)
if(cycle_pos EQUAL -1)
  message(FATAL_ERROR "missing cycle completion result [${expected_cycle}]\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
endif()

string(FIND "${mged_combined_output}" "filter" mode_pos)
if(mode_pos EQUAL -1)
  message(FATAL_ERROR "missing MGED completion mode result\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
endif()

foreach(expected_tcl_completion
    "1 {search -exec draw} draw 1 17"
    "1 {search -exec draw \"all.g\"} all.g 1 24"
    "0 {search -exec draw {}} {} 0 20"
    "0 {search -exec draw \"{}\" ;} {} 0 24")
  string(FIND "${mged_combined_output}" "${expected_tcl_completion}" tcl_completion_pos)
  if(tcl_completion_pos EQUAL -1)
    message(FATAL_ERROR "missing Tcl-aware completion result [${expected_tcl_completion}]\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
  endif()
endforeach()

foreach(expected_layout
    "layout-summary:1"
    "layout-lines:aab (3 matches)  aac (20 matches)  d5m (300 matches)"
    "layout-multiline:1")
  string(FIND "${mged_combined_output}" "${expected_layout}" layout_pos)
  if(layout_pos EQUAL -1)
    message(FATAL_ERROR "missing MGED completion layout result [${expected_layout}]\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
  endif()
endforeach()

foreach(expected_analysis
    "analysis-count:5"
    "analysis-styles:command option command valid none"
    "analysis-empty-style:invalid"
    "analysis-bare-semicolon:incomplete"
    "analysis-dynamic-command:none"
    "analysis-tcl-command:command"
    "analysis-glob-style:incomplete")
  string(FIND "${mged_combined_output}" "${expected_analysis}" analysis_pos)
  if(analysis_pos EQUAL -1)
    message(FATAL_ERROR "missing MGED analysis result [${expected_analysis}]\nstdout:\n${mged_output}\nstderr:\n${mged_error}")
  endif()
endforeach()
