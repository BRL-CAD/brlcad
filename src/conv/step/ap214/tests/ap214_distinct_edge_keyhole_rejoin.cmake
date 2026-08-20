if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, REPORT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "distinct-edge keyhole fixture returned ${import_result}, expected 0\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "removed an exact zero-area adjacent STEP edge excursion"
    "rejoined the reciprocal uses of one exact distinct-edge STEP keyhole")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" -D --exact --reject-invalid-objs
    --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR
    "strict distinct-edge keyhole fixture returned ${exact_result}, expected 3\n"
    "${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_report_text)
foreach(expected
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "closed STEP BREP did not validate as a solid")
  string(FIND "${exact_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "strict report does not contain ${expected}:\n${exact_report_text}")
  endif()
endforeach()

foreach(forbidden
    "OpenNURBS structural validation failed"
    "closed STEP BREP did not validate as a solid")
  string(FIND "${report_text}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "report unexpectedly contains ${forbidden}:\n${report_text}")
  endif()
endforeach()
