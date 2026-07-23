if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}" "${REPORT}.g" "${REPORT}.exact" "${REPORT}.exact.g")
execute_process(
  COMMAND "${STEP_G}" -D --report "${REPORT}" -o "${REPORT}.g" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_stdout
  ERROR_VARIABLE safe_stderr
  TIMEOUT 60
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR
    "near-degenerate pole safe import failed (${safe_result})\n${safe_stdout}\n${safe_stderr}")
endif()

file(READ "${REPORT}" safe_report)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "collapsed an identity-proven near-degenerate NURBS pole boundary"
    "shared STEP vertex #12")
  string(FIND "${safe_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "safe report does not contain ${expected}:\n${safe_report}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${REPORT}.exact"
    -o "${REPORT}.exact.g" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_stdout
  ERROR_VARIABLE exact_stderr
  TIMEOUT 60
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR
    "near-degenerate pole exact import should reject source geometry (got ${exact_result})\n${exact_stdout}\n${exact_stderr}")
endif()

file(READ "${REPORT}.exact" exact_report)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1")
  string(FIND "${exact_report}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact report does not contain ${expected}:\n${exact_report}")
  endif()
endforeach()
string(FIND "${exact_report}"
  "collapsed an identity-proven near-degenerate NURBS pole boundary"
  unexpected_repair)
if(NOT unexpected_repair EQUAL -1)
  message(FATAL_ERROR
    "exact mode unexpectedly applied the pole repair:\n${exact_report}")
endif()
