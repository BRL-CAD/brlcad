if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT OR NOT DEFINED REJECT_INPUT OR
    NOT DEFINED REJECT_REPORT OR NOT DEFINED ZERO_INPUT OR
    NOT DEFINED ZERO_REPORT OR NOT DEFINED ZERO_EXACT_REPORT)
  message(FATAL_ERROR
    "STEP_G and all same-vertex line inputs and reports are required")
endif()

set(reject_strict_report "${REJECT_REPORT}.strict.json")
file(REMOVE "${REPORT}" "${EXACT_REPORT}" "${REJECT_INPUT}" "${REJECT_REPORT}"
  "${reject_strict_report}" "${ZERO_INPUT}" "${ZERO_REPORT}"
  "${ZERO_EXACT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --repair safe --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR "safe same-vertex line import failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 1)
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
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0" OR
   NOT exact_report MATCHES "\"invalid_breps\":1" OR
   NOT exact_report MATCHES "\"invalid_breps_written\":1" OR
   exact_report MATCHES "removed a same-vertex STEP line edge")
  message(FATAL_ERROR
    "--exact did not preserve the invalid same-vertex source without repair:\n"
    "${exact_report}")
endif()

# Some producers author the redundant same-vertex edge itself as a zero-locus
# LINE: both its direction and VECTOR magnitude are zero and its anchor is the
# shared topology vertex.  That is stronger evidence of a removable invalid
# edge than the ordinary zero-parameter case.  Generate this producer variant
# without weakening the offset-anchor negative case below; safe mode must
# retain the valid surrounding face and exact mode must reject the source.
file(READ "${INPUT}" zero_text)
string(REPLACE
  "#22=LINE('',#1,#21);"
  "#80=DIRECTION('',(0.,0.,0.));\n#81=VECTOR('',#80,0.);\n#22=LINE('',#1,#81);"
  zero_text "${zero_text}")
file(WRITE "${ZERO_INPUT}" "${zero_text}")
execute_process(
  COMMAND "${STEP_G}" -D --repair safe --report "${ZERO_REPORT}"
    "${ZERO_INPUT}"
  RESULT_VARIABLE zero_result
  OUTPUT_VARIABLE zero_output
  ERROR_VARIABLE zero_error
)
execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${ZERO_EXACT_REPORT}"
    "${ZERO_INPUT}"
  RESULT_VARIABLE zero_exact_result
  OUTPUT_VARIABLE zero_exact_output
  ERROR_VARIABLE zero_exact_error
)
if(NOT zero_result EQUAL 0)
  message(FATAL_ERROR
    "safe zero-direction line import failed (${zero_result}):\n"
    "${zero_output}${zero_error}")
endif()
if(NOT zero_exact_result EQUAL 3)
  message(FATAL_ERROR
    "exact zero-direction line import unexpectedly returned "
    "${zero_exact_result}:\n${zero_exact_output}${zero_exact_error}")
endif()
file(READ "${ZERO_REPORT}" zero_report)
file(READ "${ZERO_EXACT_REPORT}" zero_exact_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "removed an authored zero-direction same-vertex STEP line"
    "removed a zero-locus line edge at its asserted STEP vertex")
  string(FIND "${zero_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "safe zero-direction report does not contain ${expected}:\n${zero_report}")
  endif()
endforeach()
if(NOT zero_exact_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   zero_exact_report MATCHES "removed an authored zero-direction")
  message(FATAL_ERROR
    "--exact did not retain zero-direction source failure:\n${zero_exact_report}")
endif()

# LINE.pnt is an arbitrary locator on an unbounded source line, not a segment
# endpoint.  Move that locator while leaving the single asserted topology
# vertex exactly on the line.  Default permissive mode may interpret the two
# identical projected parameters as a zero-length spur, but only in its tagged
# whole-object validation transaction.  Strict mode must retain the ambiguity
# and reject the source instead.
file(READ "${INPUT}" reject_text)
string(REPLACE
  "#22=LINE('',#1,#21);"
  "#22=LINE('',#5,#21);"
  reject_text "${reject_text}")
file(WRITE "${REJECT_INPUT}" "${reject_text}")
execute_process(
  COMMAND "${STEP_G}" -D --repair safe --report "${REJECT_REPORT}"
    "${REJECT_INPUT}"
  RESULT_VARIABLE reject_result
  OUTPUT_VARIABLE reject_output
  ERROR_VARIABLE reject_error
)
if(NOT reject_result EQUAL 0)
  message(FATAL_ERROR "permissive offset-anchor same-vertex line returned ${reject_result}:\n${reject_output}${reject_error}")
endif()
file(READ "${REJECT_REPORT}" reject_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "same_vertex_line_zero_parameter_removal"
    "LINE anchor offset")
  string(FIND "${reject_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive offset-anchor report does not contain ${expected}:\n${reject_report}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" -D --repair safe --strict
    --report "${reject_strict_report}" "${REJECT_INPUT}"
  RESULT_VARIABLE reject_strict_result
  OUTPUT_VARIABLE reject_strict_output
  ERROR_VARIABLE reject_strict_error
)
if(NOT reject_strict_result EQUAL 3)
  message(FATAL_ERROR
    "strict offset-anchor same-vertex line unexpectedly returned "
    "${reject_strict_result}:\n${reject_strict_output}${reject_strict_error}")
endif()
file(READ "${reject_strict_report}" reject_strict_text)
if(NOT reject_strict_text MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   NOT reject_strict_text MATCHES
    "refusing to remove a possibly intended segment or invent the missing topology vertex" OR
   reject_strict_text MATCHES "same_vertex_line_zero_parameter_removal")
  message(FATAL_ERROR
    "strict mode did not reject the ambiguous offset line anchor:\n"
    "${reject_strict_text}")
endif()
