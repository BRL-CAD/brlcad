if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, REPORT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}")
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
  message(FATAL_ERROR "safe collapsed-edge import failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR "exact collapsed-edge import unexpectedly returned ${exact_result}:\n${exact_output}${exact_error}")
endif()

file(READ "${REPORT}" safe_report)
file(READ "${EXACT_REPORT}" exact_report)
if(NOT safe_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0" OR
   NOT safe_report MATCHES "removed a densely validated sub-tolerance STEP edge" OR
   NOT safe_report MATCHES "removed a zero-length topology edge within model uncertainty")
  message(FATAL_ERROR "safe report does not prove the bounded topology repair:\n${safe_report}")
endif()
if(NOT exact_report MATCHES
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   exact_report MATCHES "removed a densely validated sub-tolerance STEP edge")
  message(FATAL_ERROR "--exact did not retain strict collapsed-edge behavior:\n${exact_report}")
endif()
