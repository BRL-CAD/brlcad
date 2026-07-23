if(NOT DEFINED AP214_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "AP214_G, INPUT, REPORT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${AP214_G}" -D --report "${REPORT}" "${INPUT}"
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
  message(FATAL_ERROR "safe closed-curve interval import failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR "exact closed-curve interval import failed (${exact_result}):\n${exact_output}${exact_error}")
endif()

foreach(result_file IN ITEMS "${REPORT}" "${EXACT_REPORT}")
  file(READ "${result_file}" report_text)
  foreach(expected
      "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
      "\"invalid_breps\":0")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${result_file} does not contain ${expected}:\n${report_text}")
    endif()
  endforeach()
endforeach()
