if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED MALFORMED_INPUT OR
    NOT DEFINED OUTPUT_SAFE OR NOT DEFINED OUTPUT_NONE OR
    NOT DEFINED REPORT_SAFE OR NOT DEFINED REPORT_NONE)
  message(FATAL_ERROR "STEP_G, INPUT, malformed input, outputs, and reports are required")
endif()

file(READ "${INPUT}" fixture)
set(valid_shell "#90=CLOSED_SHELL('',(#48,#58,#68,#79));")
set(malformed_shell
  "#90=CLOSED_SHELL('',(#48,#58,#68,#79,#30,#18446744073709551615));")
string(FIND "${fixture}" "${valid_shell}" shell_offset)
if(shell_offset EQUAL -1)
  message(FATAL_ERROR "direct-manifold fixture no longer has the expected shell")
endif()
string(REPLACE "${valid_shell}" "${malformed_shell}" malformed "${fixture}")
file(WRITE "${MALFORMED_INPUT}" "${malformed}")

file(REMOVE "${OUTPUT_SAFE}" "${OUTPUT_NONE}" "${REPORT_SAFE}" "${REPORT_NONE}")
execute_process(
  COMMAND "${STEP_G}" --repair safe -j 8 -O "${OUTPUT_SAFE}"
    --report "${REPORT_SAFE}" "${MALFORMED_INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
if(NOT safe_result EQUAL 0 OR NOT EXISTS "${OUTPUT_SAFE}")
  message(FATAL_ERROR
    "safe missing-face repair returned ${safe_result} or omitted output:\n"
    "${safe_output}${safe_error}")
endif()

file(READ "${REPORT_SAFE}" safe_report)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "reference to missing instance #18446744073709551615"
    "ignored a missing or non-face member in a connected face set")
  string(FIND "${safe_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe missing-face report lacks ${expected}:\n${safe_report}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" --repair none -j 8 -O "${OUTPUT_NONE}"
    --report "${REPORT_NONE}" "${MALFORMED_INPUT}"
  RESULT_VARIABLE none_result
  OUTPUT_VARIABLE none_output
  ERROR_VARIABLE none_error
)
if(NOT none_result EQUAL 3 OR EXISTS "${OUTPUT_NONE}")
  message(FATAL_ERROR
    "repair-none missing-face import returned ${none_result} or published output:\n"
    "${none_output}${none_error}")
endif()
file(READ "${REPORT_NONE}" none_report)
if(NOT none_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   none_report MATCHES "ignored a missing or non-face member")
  message(FATAL_ERROR "repair-none report does not reject the malformed shell:\n${none_report}")
endif()
