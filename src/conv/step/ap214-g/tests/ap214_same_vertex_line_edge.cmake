if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT OR NOT DEFINED REJECT_INPUT OR
    NOT DEFINED REJECT_REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, REPORT, EXACT_REPORT, REJECT_INPUT, and REJECT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}" "${REJECT_INPUT}" "${REJECT_REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D --repair safe --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR "safe same-vertex line import failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR "exact same-vertex line import unexpectedly returned ${exact_result}:\n${exact_output}${exact_error}")
endif()

file(READ "${REPORT}" safe_report)
file(READ "${EXACT_REPORT}" exact_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "removed a same-vertex STEP line edge"
    "removed a zero-parameter line edge at its asserted STEP vertex")
  string(FIND "${safe_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${safe_report}")
  endif()
endforeach()
if(NOT exact_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   exact_report MATCHES "removed a same-vertex STEP line edge")
  message(FATAL_ERROR "--exact did not retain strict same-vertex line behavior:\n${exact_report}")
endif()

# A point elsewhere on the infinite source line is not enough to prove that a
# same-vertex LINE edge is redundant.  It can be the omitted endpoint of an
# intended nonzero segment, as seen in the T90 corpus.  Move only the LINE
# anchor in a generated negative fixture and require safe mode to reject it.
file(READ "${INPUT}" reject_text)
string(REPLACE
  "#22=LINE('',#1,#21);"
  "#22=LINE('',#5,#21);"
  reject_text "${reject_text}")
file(WRITE "${REJECT_INPUT}" "${reject_text}")
execute_process(
  COMMAND "${AP214_G}" -D --repair safe --report "${REJECT_REPORT}"
    "${REJECT_INPUT}"
  RESULT_VARIABLE reject_result
  OUTPUT_VARIABLE reject_output
  ERROR_VARIABLE reject_error
)
if(NOT reject_result EQUAL 3)
  message(FATAL_ERROR "offset-anchor same-vertex line unexpectedly returned ${reject_result}:\n${reject_output}${reject_error}")
endif()
file(READ "${REJECT_REPORT}" reject_report)
if(NOT reject_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   NOT reject_report MATCHES
    "refusing to remove a possibly intended segment or invent the missing topology vertex" OR
   reject_report MATCHES "removed a same-vertex STEP line edge")
  message(FATAL_ERROR "safe mode did not conservatively reject the offset line anchor:\n${reject_report}")
endif()
