if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED EXACT_REPORT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, OUTPUT, and EXACT_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${EXACT_REPORT}")
execute_process(
  COMMAND "${AP214_G}" -v -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
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
    "using densely verified edge tolerance 0.1575"
    "adjusted one OpenNURBS edge tolerance to measured source geometry")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Open_NURBS_Measured_Mismatch_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR "open NURBS measured-mismatch BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${AP214_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(exact_result EQUAL 0)
  message(FATAL_ERROR "--exact unexpectedly accepted the out-of-tolerance source curves\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_report_text)
string(FIND "${exact_report_text}" "source curve/surface separation" found)
if(found EQUAL -1)
  message(FATAL_ERROR "--exact report did not identify the source curve/surface mismatch:\n${exact_report_text}")
endif()
