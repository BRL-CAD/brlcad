if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED EXACT_REPORT OR
   NOT DEFINED EXACT_OUTPUT)
  message(FATAL_ERROR "endpoint-anchored surface tolerance test arguments are incomplete")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${EXACT_REPORT}" "${EXACT_OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "safe ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "source edge/surface geometry exceeded the declared tolerance"
    "used an endpoint-anchored measured tolerance for interior source curve/surface drift")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Endpoint_Anchored_Surface_Tolerance_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4")
  message(FATAL_ERROR "endpoint-anchored surface tolerance BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${AP214_G}" --exact -O "${EXACT_OUTPUT}" --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(exact_result EQUAL 0)
  message(FATAL_ERROR "--exact unexpectedly accepted the source curve/surface mismatch\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_report_text)
string(FIND "${exact_report_text}" "exact model tolerance" exact_found)
if(exact_found EQUAL -1)
  message(FATAL_ERROR "--exact report did not identify the source curve/surface mismatch:\n${exact_report_text}")
endif()
