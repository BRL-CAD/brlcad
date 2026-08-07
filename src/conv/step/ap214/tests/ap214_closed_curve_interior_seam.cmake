if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, REPORT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${REPORT}" "${INPUT}"
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
  message(FATAL_ERROR "safe closed-curve seam relocation failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR "exact closed-curve seam relocation failed (${exact_result}):\n${exact_output}${exact_error}")
endif()

file(READ "${REPORT}" safe_report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":0"
    "\"diagnostics\":[]")
  string(FIND "${safe_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${REPORT} does not contain ${expected}:\n${safe_report_text}")
  endif()
endforeach()

# Safe mode must retain the already valid source topology instead of moving its
# closed curve seam speculatively.  --exact deliberately bypasses that
# unrepaired preflight and continues to exercise the bounded seam relocation
# path, so this fixture guards both decisions.
file(READ "${EXACT_REPORT}" exact_report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":0"
    "relocated the exact closed source-curve seam to its asserted STEP topology vertex")
  string(FIND "${exact_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${EXACT_REPORT} does not contain ${expected}:\n${exact_report_text}")
  endif()
endforeach()
